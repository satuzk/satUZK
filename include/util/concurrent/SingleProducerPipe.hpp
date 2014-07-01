
#include <cassert>
#include <atomic>
#include <new>

namespace util {
namespace concurrent {

/* Implements a wait-free single producer shared-memory pipe.
 *
 * NOTE: it is very important that all write() operators are paired
 * with corresponding read() operation of the same data type.
 * e.g. read<char>() preceeded by write<int>() will return invalid/random data */
template<int Size = 4080>
class SingleProducerPipe {
private:
	typedef uint64_t Throughput;
	
	struct Block {
		uint8_t data[Size];
		Block *successor;
		std::atomic<int> releaseCount;
		
		Block() : successor(nullptr), releaseCount(0) { }
	};
	
public:
	class Producer {
	public:
		Producer(SingleProducerPipe &queue) : p_queue(queue),
				p_block(queue.p_first), p_offset(0), p_writeable(0)  {
		}
		Producer(const Producer &other) = delete;
		Producer &operator= (const Producer &other) = delete;
		
		template<typename T>
		void write(const T &object) {
			static_assert(sizeof(T) < Size, "Type T is too large");
			uint32_t align = alignof(T) - (p_offset % alignof(T));
			p_offset += align;
			p_writeable += align;

			if(p_offset + sizeof(T) > Size) {
				p_writeable += Size - p_offset;
				Block *block = new Block();
				p_block->successor = block;
				p_block = block;
				p_offset = 0;
			}

			assert(p_offset + sizeof(T) <= Size);
			assert(reinterpret_cast<uintptr_t>(p_block->data + p_offset) % alignof(T) == 0);
			uint8_t *ptr = p_block->data + p_offset;
			new (ptr) T(object);
			p_offset += sizeof(T);
			p_writeable += sizeof(T);
		}
		
		void send() {
			p_queue.p_tail.fetch_add(p_writeable);
			p_writeable = 0;
		}
	private:
		SingleProducerPipe &p_queue;
		Block *p_block;
		uint32_t p_offset;
		Throughput p_writeable;
	};
	
	class Consumer {
	public:
		/* NOTE: this constructor is not thread-safe!
		 * furthermore all consumers have to be created before the first byte
		 * of data is written to the pipe! */
		Consumer(SingleProducerPipe &queue) : p_queue(queue),
				p_block(queue.p_first), p_offset(0), p_lastTail(0), p_readHead(0) {
			queue.p_numConsumers++;
		}
		// NOTE: never use the original Consumer after calling this move constructor!
		Consumer(Consumer &&other) : p_queue(other.p_queue), p_block(other.p_block),
				p_offset(other.p_offset), p_lastTail(other.p_lastTail), p_readHead(other.p_readHead) {
		}
		Consumer(const Consumer &other) = delete;
		Consumer &operator= (const Consumer &other) = delete;
		
		template<typename T>
		T read() {
			static_assert(sizeof(T) < Size, "Type T is too large");
			uint32_t align = alignof(T) - (p_offset % alignof(T));
			p_offset += align;
			p_readHead += align;

			if(p_offset + sizeof(T) > Size) {
				p_readHead += Size - p_offset;
				Block *block = p_block->successor;
				if(block->releaseCount.fetch_add(1) == p_queue.p_numConsumers - 1)
					delete p_block;
				p_block = block;
				p_offset = 0;
			}
			
			assert(p_offset + sizeof(T) <= Size);
			assert(reinterpret_cast<uintptr_t>(p_block->data + p_offset) % alignof(T) == 0);
			assert(p_readHead + sizeof(T) <= p_lastTail);
			uint8_t *ptr = p_block->data + p_offset;
			T object = *reinterpret_cast<T*>(ptr);
			p_offset += sizeof(T);
			p_readHead += sizeof(T);
			return object;
		}
		
		bool receive() {
			// avoid atomic operations if there is data left
			if(p_lastTail - p_readHead > 0)
				return true;
			p_lastTail = p_queue.p_tail.load();
			return p_lastTail - p_readHead > 0;
		}
	private:
		SingleProducerPipe &p_queue;
		Block *p_block;
		uint32_t p_offset;
		Throughput p_lastTail;
		Throughput p_readHead;
	};
	
	SingleProducerPipe() : p_tail(0), p_numConsumers(0) {
		p_first = new Block();
	}
	
private:
	Block *p_first;
	std::atomic<Throughput> p_tail;
	int p_numConsumers;
};

}}; // namespace util::concurrent

