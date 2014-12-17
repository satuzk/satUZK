
namespace satuzk {

class ExtModelConfigStruct {
public:
	template<typename Hooks>
	void pushFact(Hooks &hooks, typename Hooks::Literal fact);
	
	template<typename Hooks>
	void pushEquivalent(Hooks &hooks, typename Hooks::Variable variable,
			typename Hooks::Literal zero_equivalent);
	
	template<typename Hooks>
	void pushDistributed(Hooks &hooks, typename Hooks::Variable variable);
	
	template<typename Hooks>
	void pushBlockedClause(Hooks &hooks, typename Hooks::Clause clause,
			typename Hooks::Literal blocking);

	template<typename Hooks, typename ModelMap>
	void buildModel(Hooks &hooks, ModelMap &model);
	
	size_t getUsedSpace() {
		return p_stack.size() * sizeof(Word);
	}

private:
	typedef uint32_t Word;

	enum TagType {
		kTagNone,
		kTagFact,
		kTagEquivalent,
		kTagDistributed,
		kTagBlockedClause
	};

	template<typename Hooks, typename ModelMap>
	bool checkStoredClause(Hooks &hooks, std::vector<Word>::iterator &it, ModelMap &model);

	template<typename Hooks, typename ModelMap>
	void extendEquivalent(Hooks &hooks, std::vector<Word>::iterator &it, ModelMap &model);

	template<typename Hooks, typename ModelMap>
	void extendDistributed(Hooks &hooks, std::vector<Word>::iterator &it, ModelMap &model);

	template<typename Hooks, typename ModelMap>
	void extendBlockedClause(Hooks &hooks, std::vector<Word>::iterator &it, ModelMap &model);

	std::vector<Word> p_stack;
};

template<typename Hooks>
void ExtModelConfigStruct::pushFact(Hooks &hooks, typename Hooks::Literal fact) {
	p_stack.push_back(fact.getIndex());
	p_stack.push_back(kTagFact);
}

template<typename Hooks>
void ExtModelConfigStruct::pushEquivalent(Hooks &hooks, typename Hooks::Variable variable,
		typename Hooks::Literal zero_equivalent) {
	p_stack.push_back(zero_equivalent.getIndex());
	p_stack.push_back(variable.getIndex());
	p_stack.push_back(kTagEquivalent);
}

template<typename Hooks>
void ExtModelConfigStruct::pushDistributed(Hooks &hooks, typename Hooks::Variable variable) {
	// NOTE: we only have to save the clauses containing the zero literal
	auto zero_literal = variable.zeroLiteral();

	Word count = 0;
	for(auto j = hooks.occurBegin(zero_literal); j != hooks.occurEnd(zero_literal); ++j) {
		if(!hooks.clauseIsPresent(*j))
			continue;
		for(auto i = hooks.clauseBegin(*j); i != hooks.clauseEnd(*j); ++i)
			p_stack.push_back((*i).getIndex());
		p_stack.push_back(hooks.clauseLength(*j));
		count++;
	}
	p_stack.push_back(count);

	p_stack.push_back(variable.getIndex());
	p_stack.push_back(kTagDistributed);
}

template<typename Hooks>
void ExtModelConfigStruct::pushBlockedClause(Hooks &hooks, typename Hooks::Clause clause,
		typename Hooks::Literal blocking) {
	for(auto i = hooks.clauseBegin(clause); i != hooks.clauseEnd(clause); ++i)
		p_stack.push_back((*i).getIndex());
	p_stack.push_back(hooks.clauseLength(clause));

	p_stack.push_back(blocking.getIndex());
	p_stack.push_back(kTagBlockedClause);
}

template<typename Hooks, typename ModelMap>
bool ExtModelConfigStruct::checkStoredClause(Hooks &hooks,
		std::vector<Word>::iterator &it, ModelMap &model) {
	bool satisfied = false;
	Word length = *(--it);
	for(Word i = 0; i < length; ++i) {
		auto lit = Hooks::Literal::fromIndex(*(--it));
	
		Bool3 var_state = model[lit.variable().toNumber()];
		if(lit.isOneLiteral() ? var_state.isTrue() : var_state.isFalse())
			satisfied = true;
	}
	return satisfied;
}

template<typename Hooks, typename ModelMap>
void ExtModelConfigStruct::extendEquivalent(Hooks &hooks,
		std::vector<Word>::iterator &it, ModelMap &model) {
	auto var = Hooks::Variable::fromIndex(*(--it));
	auto zero_equivalent = Hooks::Literal::fromIndex(*(--it));

	Bool3 var_state = model[zero_equivalent.variable().toNumber()];
	if(zero_equivalent.isOneLiteral() ? var_state.isTrue() : var_state.isFalse()) {
		model[var.toNumber()] = false3();
	}else model[var.toNumber()] = true3();
}

template<typename Hooks, typename ModelMap>
void ExtModelConfigStruct::extendDistributed(Hooks &hooks,
		std::vector<Word>::iterator &it, ModelMap &model) {
	// try to set the variable to one first
	auto var = Hooks::Variable::fromIndex(*(--it));
	model[var.toNumber()] = true3();

	Word count = *(--it);
	bool satisfied = true;
	for(Word i = 0; i < count; ++i) {
		if(!checkStoredClause(hooks, it, model))
			satisfied = false;
	}
	
	// that did not satisfy the formula, so the variable must be zero
	if(!satisfied)
		model[var.toNumber()] = false3();
}

template<typename Hooks, typename ModelMap>
void ExtModelConfigStruct::extendBlockedClause(Hooks &hooks,
		std::vector<Word>::iterator &it, ModelMap &model) {
	auto blocking = Hooks::Literal::fromIndex(*(--it));

	// check the current model first
	if(checkStoredClause(hooks, it, model))
		return;
	// didn't satisfy the clause so the blocking literal must be set
	model[blocking.variable().toNumber()] = blocking.isOneLiteral() ? true3() : false3();
}

template<typename Hooks, typename ModelMap>
void ExtModelConfigStruct::buildModel(Hooks &hooks, ModelMap &model) {
	// set all model assignments to the current assignment
	for(auto it = hooks.varsBegin(); it != hooks.varsEnd(); ++it) {
		if(!hooks.varAssigned(*it))
			continue;
		model[(*it).toNumber()] = hooks.varOne(*it) ? true3() : false3();
	}

	for(auto it = p_stack.end(); it != p_stack.begin(); ) {
		Word tag = *(--it);

		if(tag == TagType::kTagFact) {
			auto literal = Hooks::Literal::fromIndex(*(--it));
			model[literal.variable().toNumber()] = literal.isOneLiteral() ? true3() : false3();
		}else if(tag == TagType::kTagEquivalent) {
			extendEquivalent(hooks, it, model);
		}else if(tag == TagType::kTagDistributed) {
			extendDistributed(hooks, it, model);
		}else if(tag == TagType::kTagBlockedClause) {
			extendBlockedClause(hooks, it, model);
		}else SYS_CRITICAL("Illegal tag\n");
	}
}

}; // namespace satuzk

