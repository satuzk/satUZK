
/* Concept: HeapHooks
	Required types:
		Index
		Item
	
	Required functions:
		void initHeapIndex(Item item, Index index);
		void updateHeapIndex(Item item, Index index);
		void removeHeapIndex(Item item);
		Index getHeapIndex(Item item);
		
		bool isLess(Item item1, Item item2);
		
		void setHeapArray(Index index, Item item);
		Item getHeapArray(Index index);

		Index getArraySize()
		Index incArraySize()
		Index decArraySize()
*/

namespace util {
namespace binary_heap {

// get index of parent node
template<typename HeapHooks>
typename HeapHooks::Index p_parent(typename HeapHooks::Index index) {
	SYS_ASSERT(SYS_ASRT_GENERAL, index != 0);
	return (index - 1) / 2;
}

// get index of left child
template<typename HeapHooks>
typename HeapHooks::Index p_left(typename HeapHooks::Index index) {
	return index * 2 + 1;
}

// get index of right child
template<typename HeapHooks>
typename HeapHooks::Index p_right(typename HeapHooks::Index index) {
	return index * 2 + 2;
}

// fixes the heap property when the given node might be less than its parent
template<typename HeapHooks>
void p_fixUp(HeapHooks &hooks, typename HeapHooks::Index index) {
	SYS_ASSERT(SYS_ASRT_GENERAL, index < hooks.getArraySize());
	if(index == 0)
		return;
	
	// swap node with parent; then fix parent
	typename HeapHooks::Index p_index = p_parent<HeapHooks>(index);
	typename HeapHooks::Item item = hooks.getHeapArray(index);
	typename HeapHooks::Item parent = hooks.getHeapArray(p_index);

	if(hooks.isLess(parent, item))
		return;
		
	hooks.setHeapArray(p_index, item);
	hooks.setHeapArray(index, parent);
	hooks.updateHeapIndex(item, p_index);
	hooks.updateHeapIndex(parent, index);
	p_fixUp<HeapHooks>(hooks, p_index);
}

// fixes the heap property when children of the given node might be less than the node
template<typename HeapHooks>
void p_fixDown(HeapHooks &hooks, typename HeapHooks::Index index) {
	SYS_ASSERT(SYS_ASRT_GENERAL, index < hooks.getArraySize());

	// swap node with minimal child; then fix child
	typename HeapHooks::Index l_index = p_left<HeapHooks>(index);
	typename HeapHooks::Index r_index = p_right<HeapHooks>(index);

	// case 1: the item has no children
	if(!(l_index < hooks.getArraySize()))
		return;
	
	// case 2: the item has only a left child
	typename HeapHooks::Item item = hooks.getHeapArray(index);
	typename HeapHooks::Item left = hooks.getHeapArray(l_index);
	if(!(r_index < hooks.getArraySize())) {
		if(hooks.isLess(item, left))
			return;

		hooks.setHeapArray(l_index, item);
		hooks.setHeapArray(index, left);
		hooks.updateHeapIndex(item, l_index);
		hooks.updateHeapIndex(left, index);
		return;
	}

	// case 3: the item has two children
	typename HeapHooks::Item right = hooks.getHeapArray(r_index);

	bool left_less = hooks.isLess(left, right);
	typename HeapHooks::Index m_index = left_less ? l_index : r_index;
	typename HeapHooks::Item &minimal = left_less ? left : right;
	if(hooks.isLess(item, minimal))
		return;

	hooks.setHeapArray(m_index, item);
	hooks.setHeapArray(index, minimal);
	hooks.updateHeapIndex(item, m_index);
	hooks.updateHeapIndex(minimal, index);
	p_fixDown(hooks, m_index);
}

template<typename HeapHooks>
void insert(HeapHooks &hooks, typename HeapHooks::Item item) {
	// insert as last node; then fix heap
	typename HeapHooks::Index index = hooks.getArraySize();
	hooks.incArraySize();
	hooks.setHeapArray(index, item);
	hooks.initHeapIndex(item, index);
		
	p_fixUp(hooks, index);
}

template<typename HeapHooks>
void idxBecauseLess(HeapHooks &hooks, typename HeapHooks::Index index) {
	p_fixUp(hooks, index);
}

template<typename HeapHooks>
void becameLess(HeapHooks &hooks, typename HeapHooks::Item item) {
	typename HeapHooks::Index index = hooks.getHeapIndex(item);
	idxBecauseLess(hooks, index);
}

template<typename HeapHooks>
void p_checkIntegrity(HeapHooks &hooks, typename HeapHooks::Index index) {
	typename HeapHooks::Index l_index = p_left<HeapHooks>(index);
	typename HeapHooks::Index r_index = p_right<HeapHooks>(index);

	if(l_index < hooks.getArraySize()) {
		SYS_ASSERT(SYS_ASRT_GENERAL, !hooks.isLess(hooks.getHeapArray(l_index),
				hooks.getHeapArray(index)));
		p_checkIntegrity(hooks, l_index);
	}
	if(r_index < hooks.getArraySize()) {
		SYS_ASSERT(SYS_ASRT_GENERAL, !hooks.isLess(hooks.getHeapArray(r_index),
				hooks.getHeapArray(index)));
		p_checkIntegrity(hooks, r_index);
	}
}

template<typename HeapHooks>
void checkIntegrity(HeapHooks &hooks) {
	if(hooks.getArraySize() > 0)
		p_checkIntegrity(hooks, 0);
}

template<typename HeapHooks>
void idxRemove(HeapHooks &hooks, typename HeapHooks::Index index) {
	// if we are removing the last node no fixes are necessary
	hooks.decArraySize();
	typename HeapHooks::Item item = hooks.getHeapArray(index);
	typename HeapHooks::Index last_index = hooks.getArraySize();
	if(index == last_index) {
		hooks.removeHeapIndex(item);
		return;
	}

	// swap last node to the given node; then fix heap
	typename HeapHooks::Item last = hooks.getHeapArray(last_index);
	hooks.setHeapArray(index, last);
	hooks.updateHeapIndex(last, index);
	hooks.removeHeapIndex(item);
	
	p_fixDown(hooks, index);
}

template<typename HeapHooks>
typename HeapHooks::Item getMinimum(HeapHooks &hooks) {
	SYS_ASSERT(SYS_ASRT_GENERAL, hooks.getArraySize() > 0);
	return hooks.getHeapArray(0);
}

template<typename HeapHooks>
typename HeapHooks::Item removeMinimum(HeapHooks &hooks) {
	typename HeapHooks::Item minimum = getMinimum(hooks);
	idxRemove(hooks, 0);
	return minimum;
}

}}; // namespace util::binary_heap


