
namespace util {
namespace memory {

template<typename Index>
class BulkAllocator {
public:
	BulkAllocator(Index alignment) : p_memory(NULL), p_alignment(alignment),
			p_offset(0), p_length(0) { };
	BulkAllocator(const BulkAllocator &other) = delete;
	BulkAllocator &operator= (const BulkAllocator &other) = delete;
	
	BulkAllocator &operator= (BulkAllocator &&other) {
		p_memory = other.p_memory;
		p_alignment = other.p_alignment;
		p_offset = other.p_offset;
		p_length = other.p_length;
		return *this;
	}
	
	void initMemory(void *pointer, Index length) {
		SYS_ASSERT(SYS_ASRT_GENERAL, p_memory == NULL);
		SYS_ASSERT(SYS_ASRT_GENERAL, pointer != NULL);

		p_memory = pointer;
		p_length = length;
		p_offset = 0;
	}
	void moveTo(void *pointer, Index length) {
		std::memcpy(pointer, p_memory, p_offset);
		p_memory = pointer;
		p_length = length;
	}
	void *getPointer() {
		return p_memory;
	}
	Index getTotalSpace() {
		return p_length;
	}
	Index getFreeSpace() {
		return p_length - p_offset;
	}
	Index getUsedSpace() {
		return p_offset;
	}

	Index alloc(Index length) {
		Index index = p_offset;
		if(index % p_alignment != 0)
			index += p_alignment - index % p_alignment;
		SYS_ASSERT(SYS_ASRT_GENERAL, index + length < p_length);
		p_offset = index + length;
		return index;
	}

	void *operator[] (Index index) {
		return (void*)((uintptr_t)p_memory + index);
	}
		
private:
	void *p_memory;
	Index p_alignment;
	Index p_offset;
	Index p_length;
};

}}; /* namespace util::memory */

