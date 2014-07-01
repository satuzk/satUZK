
namespace satuzk {


template<typename BaseDefs>
class VsidsConfigStruct {
private:
	typedef VariableType<BaseDefs> Variable;
	typedef typename BaseDefs::Activity Activity;
	typedef uint32_t HeapIndex;
	
	struct HeapInfo {
		Activity activity;
		HeapIndex index;
	};

	class HeapHooksStruct {
	public:
		typedef Variable Item;
		typedef HeapIndex Index;
		
		static const uint32_t kIllegalIndex = (HeapIndex)(-1);

		HeapHooksStruct(VsidsConfigStruct &config) : p_config(config) { }

		bool isLess(Item left, Item right) {
			// NOTE: we want a maximum heap so we are comparing for ">"
			return p_config.getActivity(left) > p_config.getActivity(right);
		}

		void initHeapIndex(Item variable, HeapIndex index) {
			SYS_ASSERT(SYS_ASRT_GENERAL, p_config.p_varInfos[variable.getIndex()].index == kIllegalIndex);
			p_config.p_varInfos[variable.getIndex()].index = index;
		}
		void updateHeapIndex(Item variable, HeapIndex index) {
			SYS_ASSERT(SYS_ASRT_GENERAL, p_config.p_varInfos[variable.getIndex()].index != kIllegalIndex);
			p_config.p_varInfos[variable.getIndex()].index = index;
		}
		void removeHeapIndex(Item variable) {
			SYS_ASSERT(SYS_ASRT_GENERAL, p_config.p_varInfos[variable.getIndex()].index != kIllegalIndex);
			p_config.p_varInfos[variable.getIndex()].index = kIllegalIndex;
		}

		HeapIndex getHeapIndex(Item variable) {
			return p_config.p_varInfos[variable.getIndex()].index;
		}

		uint32_t getArraySize() { return p_config.p_heapSize; }
		void incArraySize() { p_config.p_heapSize++; }
		void decArraySize() { p_config.p_heapSize--; }

		Item getHeapArray(HeapIndex index) {
			return p_config.p_heapArray[index];
		}
		void setHeapArray(HeapIndex index, Item variable) {
			p_config.p_heapArray[index] = variable;
		}

	private:
		VsidsConfigStruct &p_config;
	};
	
public:
	void onAllocVariable(Variable variable) {
		HeapInfo var_info;
		var_info.activity = 0;
		var_info.index = HeapHooksStruct::kIllegalIndex;
		p_varInfos.push_back(var_info);
	
		p_heapArray.push_back(Variable::illegalVar());
		
		HeapHooksStruct heap_hooks(*this);
		util::binary_heap::insert(heap_hooks, variable);
	}
	
	Activity getActivity(Variable variable) {
		return p_varInfos[variable.getIndex()].activity;
	}
	
	Activity incActivity(Variable variable, Activity value) {
		HeapHooksStruct heap_hooks(*this);
		
		Activity activity = p_varInfos[variable.getIndex()].activity + value;
		p_varInfos[variable.getIndex()].activity = activity;
		
		if(p_varInfos[variable.getIndex()].index != HeapHooksStruct::kIllegalIndex)
			util::binary_heap::becameLess(heap_hooks, variable);
		
		return activity;
	}

	void scaleActivity(Activity divisor) {
		for(auto it = p_varInfos.begin(); it != p_varInfos.end(); ++it)
			it->activity /= divisor;
	}

	void insertVariable(Variable variable) {
		HeapHooksStruct heap_hooks(*this);
		util::binary_heap::insert(heap_hooks, variable);
	}

	bool isInserted(Variable variable) {
		return p_varInfos[variable.getIndex()].index != HeapHooksStruct::kIllegalIndex;
	}
	
	Variable removeMaximum() {
		HeapHooksStruct heap_hooks(*this);
		return util::binary_heap::removeMinimum(heap_hooks);
	}
	
private:
	std::vector<HeapInfo> p_varInfos;
	std::vector<Variable> p_heapArray;
	uint32_t p_heapSize;
};

}; // namespace satuzk

