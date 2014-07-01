
namespace util {

class BinaryStack {
public:
	typedef unsigned int Size;

	BinaryStack() : p_pointer(NULL), p_offset(0), p_capacity(0) { }
	~BinaryStack() {
		operator delete(p_pointer);
	}
	
	Size allocate(Size length) {
		if(p_pointer == NULL || p_capacity < p_offset + length) {
			Size new_capacity = 1.5f * (p_offset + length);
			void *new_pointer = operator new(new_capacity);
			if(p_pointer != NULL)
				std::memcpy(new_pointer, p_pointer, p_capacity);
			operator delete(p_pointer);
			p_pointer = new_pointer;
			p_capacity = new_capacity;
		}
		Size offset = p_offset;
		p_offset += length;
		return offset;
	}
	void align(Size alignment) {
		Size over = p_offset % alignment;
		if(over != 0)
			p_offset += (alignment - over);
	}

	template<typename T>
	T *access(Size offset) {
		auto pointer = reinterpret_cast<uintptr_t>(p_pointer);
		return reinterpret_cast<T*>	(pointer + offset);
	}

	Size getOffset() {
		return p_offset;
	}

private:
	void *p_pointer;
	Size p_offset;
	Size p_capacity;
};

class BinaryStackWriter {
public:
	typedef unsigned int Size;

	BinaryStackWriter(BinaryStack &alloc) : p_allocator(alloc) {
		p_baseOffset = p_allocator.getOffset();
	}
	
	Size baseOffset() {
		return p_baseOffset;
	}
	
	template<typename T>
	void write(T object) {
		p_allocator.align(__alignof__(T));
		Size offset = p_allocator.allocate(sizeof(T));
		*p_allocator.template access<T>(offset) = object;
	}
	
private:
	BinaryStack &p_allocator;
	Size p_baseOffset;
};

class BinaryStackReader {
public:
	typedef unsigned int Size;
	
	BinaryStackReader(BinaryStack &alloc, Size baseOffset)
			: p_allocator(alloc), p_offset(baseOffset) { }

	template<typename T>
	T read() {
		Size alignment = __alignof__(T);
		Size over = p_offset % alignment;
		if(over != 0)
			p_offset += (alignment - over);
		T *pointer = p_allocator.template access<T>(p_offset);
		p_offset += sizeof(T);
		return *pointer;
	}
private:
	BinaryStack &p_allocator;
	Size p_offset;
};

};

