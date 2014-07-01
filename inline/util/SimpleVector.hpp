
namespace util {
namespace vectors {

/* note: this class is a POD and assumes that the type T is a POD too. */
template<typename T, int GrowNumerator = 2, int GrowDenominator = 1>
struct simple {
	typedef uint32_t size_type;

	struct iterator : public std::iterator<std::forward_iterator_tag, T> {
			iterator(T *pointer) : p_pointer(pointer) { }
			
			size_type operator- (const iterator other) const {
				return p_pointer - other.p_pointer;
			}
			void operator++() {
				p_pointer++;
			}
			void operator--() {
				p_pointer--;
			}
			bool operator==(iterator other) {
				return p_pointer == other.p_pointer;
			}
			bool operator!=(iterator other) {
				return p_pointer != other.p_pointer;
			}
			T &operator* () {
				return *p_pointer;
			}
			
			T *p_pointer;
	};

	simple() {
		p_pointer = NULL;
		p_size = 0;
		p_capacity = 0;
	}
	simple(simple &&other) : p_pointer(other.p_pointer), p_size(other.p_size),
			p_capacity(other.p_capacity) {
		other.p_pointer = NULL;
		other.p_size = 0;
		other.p_capacity = 0;
	}
	~simple() {
		if(p_pointer != NULL)
			operator delete(p_pointer);
	}

	inline void ensure_capacity(size_type new_size) {
		if(__builtin_expect(new_size < p_capacity, 1))
			return;
		if(p_pointer == NULL) {
			size_type new_cap = (new_size * GrowNumerator) / GrowDenominator;
			size_type bytes = sizeof(T) * new_cap;
			T *new_pointer = reinterpret_cast<T*>(operator new(bytes));
			p_pointer = new_pointer;
			p_capacity = new_cap;
		}else{
//			std::cout << "Resize: " << p_capacity << " to " << new_size << std::endl;
			size_type new_cap = (new_size * GrowNumerator) / GrowDenominator;
			size_type bytes = sizeof(T) * new_cap;
			T *new_pointer = reinterpret_cast<T*>(operator new(bytes));
			std::memcpy(new_pointer, p_pointer, sizeof(T) * p_size);
			operator delete(p_pointer);
			p_pointer = new_pointer;
			p_capacity = new_cap;
		}
	}

	void reserve(size_type count) {
		ensure_capacity(count);
	}

	size_type size() {
		return p_size;
	}

	iterator begin() {
		return iterator(p_pointer);
	}
	iterator end() {
		return iterator(p_pointer + p_size);
	}

	size_type index_of(iterator it) {
		return it.p_pointer - p_pointer;
	}

	void push_back(T value) {
		ensure_capacity(p_size + 1);
		p_pointer[p_size++] = value; 
	}
	void pop_back() {
		p_size--;
	}
	void erase(iterator it) {
		for(size_type i = index_of(it); i < p_size - 1; i++)
			p_pointer[i] = p_pointer[i + 1];
		p_size--;
	}

	void resize(size_type new_size) {
		p_size = new_size;
	}
	void clear() {
		resize(0);
	}

	void shrink_to_fit() {
		if(p_size == 0) {
			if(p_pointer)
				operator delete(p_pointer);
			p_pointer = NULL;
			p_capacity = p_size;
		}else{
			size_type bytes = sizeof(T) * p_size;
			T *new_pointer = reinterpret_cast<T*>(operator new(bytes));
			std::memcpy(new_pointer, p_pointer, sizeof(T) * p_size);
			operator delete(p_pointer);
			p_pointer = new_pointer;
			p_capacity = p_size;
		}
	}

	T *p_pointer;
	size_type p_size;
	size_type p_capacity;
};

}};

