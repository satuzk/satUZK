
namespace satuzk {

enum TagType {
	kTagNone,
	kTagFact,
	kTagEquivalent,
	kTagDistributed,
	kTagBlockedClause
};

struct Descriptor {
	TagType tag;
	uint32_t baseOffset;
};

struct ExtModelConfigStruct {
	std::vector<Descriptor> p_descriptorStack;
	util::BinaryStack p_dataStack;

	void push(TagType tag, uint32_t baseOffset) {
		Descriptor description;
		description.tag = tag;
		description.baseOffset = baseOffset;
		p_descriptorStack.push_back(description);
	}

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

	template<typename Hooks>
	bool checkStoredClause(Hooks &hooks, util::BinaryStackReader &reader);

	template<typename Hooks>
	void extendEquivalent(Hooks &hooks, uint32_t base_offset);

	template<typename Hooks>
	void extendDistributed(Hooks &hooks, uint32_t base_offset);

	template<typename Hooks>
	void extendBlockedClause(Hooks &hooks, uint32_t base_offset);

	template<typename Hooks>
	void buildModel(Hooks &hooks);
};

template<typename Hooks>
void ExtModelConfigStruct::pushFact(Hooks &hooks, typename Hooks::Literal fact) {
	util::BinaryStackWriter writer(p_dataStack);
	writer.template write<uint32_t>(fact);
	push(TagType::kTagFact, writer.baseOffset());
}

template<typename Hooks>
void ExtModelConfigStruct::pushEquivalent(Hooks &hooks, typename Hooks::Variable variable,
		typename Hooks::Literal zero_equivalent) {
	util::BinaryStackWriter writer(p_dataStack);
	writer.template write<uint32_t>(variable.getIndex());
	writer.template write<uint32_t>(zero_equivalent.getIndex());
	push(TagType::kTagEquivalent, writer.baseOffset());
}

template<typename Hooks>
void ExtModelConfigStruct::pushDistributed(Hooks &hooks, typename Hooks::Variable variable) {
	// NOTE: we only have to save the clauses containing the zero literal
	auto zero_literal = variable.zeroLiteral();
	
	util::BinaryStackWriter writer(p_dataStack);
	writer.template write<uint32_t>(variable.getIndex());

	uint32_t count = 0;
	for(auto j = hooks.occurBegin(zero_literal); j != hooks.occurEnd(zero_literal); ++j) {
		if(!hooks.clauseIsPresent(*j))
			continue;
		count++;
	}
	writer.template write<uint32_t>(count);

	for(auto j = hooks.occurBegin(zero_literal); j != hooks.occurEnd(zero_literal); ++j) {
		if(!hooks.clauseIsPresent(*j))
			continue;
		writer.template write<uint32_t>(hooks.clauseLength(*j));
		for(auto i = hooks.clauseBegin(*j); i != hooks.clauseEnd(*j); ++i)
			writer.template write<uint32_t>((*i).getIndex());
	}
	push(TagType::kTagDistributed, writer.baseOffset());
}

template<typename Hooks>
void ExtModelConfigStruct::pushBlockedClause(Hooks &hooks, typename Hooks::Clause clause,
		typename Hooks::Literal blocking) {
	util::BinaryStackWriter writer(p_dataStack);
	writer.template write<uint32_t>(blocking.getIndex());
	writer.template write<uint32_t>(hooks.clauseLength(clause));
	for(auto i = hooks.clauseBegin(clause);
			i != hooks.clauseEnd(clause); ++i) {
		writer.template write<uint32_t>((*i).getIndex());
	}
	push(TagType::kTagBlockedClause, writer.baseOffset());
}

template<typename Hooks>
bool ExtModelConfigStruct::checkStoredClause(Hooks &hooks, util::BinaryStackReader &reader) {
	bool satisfied = false;
	uint32_t length = reader.template read<uint32_t>();
	for(uint32_t i = 0; i < length; ++i) {
		auto lit = Hooks::Literal::fromIndex(reader.template read<uint32_t>());
		if(hooks.modelGetLiteral(lit))
			satisfied = true;
	}
	return satisfied;
}

template<typename Hooks>
void ExtModelConfigStruct::extendEquivalent(Hooks &hooks, uint32_t base_offset) {
	util::BinaryStackReader reader(p_dataStack, base_offset);
	
	auto var = Hooks::Variable::fromIndex(reader.template read<uint32_t>());
	auto zero_equivalent = Hooks::Literal::fromIndex(reader.template read<uint32_t>());

	if(hooks.modelGetLiteral(zero_equivalent)) {
		hooks.modelSetVariable(var, false);
	}else hooks.modelSetVariable(var, true);
}

template<typename Hooks>
void ExtModelConfigStruct::extendDistributed(Hooks &hooks, uint32_t base_offset) {
	util::BinaryStackReader reader(p_dataStack, base_offset);

	// try to set the variable to one first
	auto var = Hooks::Variable::fromIndex(reader.template read<uint32_t>());
	hooks.modelSetVariable(var, true);

	uint32_t count = reader.template read<uint32_t>();
	for(uint32_t i = 0; i < count; ++i) {
		if(checkStoredClause(hooks, reader))
			continue;
		// that did not satisfy the formula, so the variable must be zero
		hooks.modelSetVariable(var, false);
		return;
	}
}

template<typename Hooks>
void ExtModelConfigStruct::extendBlockedClause(Hooks &hooks, uint32_t base_offset) {
	util::BinaryStackReader reader(p_dataStack, base_offset);

	auto blocking = Hooks::Literal::fromIndex(reader.template read<uint32_t>());

	// check the current model first
	if(checkStoredClause(hooks, reader))
		return;
	// didn't satisfy the clause so the blocking literal must be set
	hooks.modelSetVariable(blocking.variable(), blocking.isOneLiteral());
}

template<typename Hooks>
void ExtModelConfigStruct::buildModel(Hooks &hooks) {
	// set all model assignments to the current assignment
	for(auto it = hooks.varsBegin(); it != hooks.varsEnd(); ++it) {
		if(!hooks.varAssigned(*it))  {
			// TODO: memorize unassigned variables
			hooks.modelSetVariable(*it, false);
		}else hooks.modelSetVariable(*it, hooks.varOne(*it));
	}

	for(auto i = p_descriptorStack.rbegin(); i != p_descriptorStack.rend(); ++i) {
		util::BinaryStackReader reader(p_dataStack, (*i).baseOffset);
		if((*i).tag == TagType::kTagFact) {
			auto literal = Hooks::Literal::fromIndex(reader.template read<uint32_t>());
			hooks.modelSetVariable(literal.variable(), literal.isOneLiteral());
		}else if((*i).tag == TagType::kTagEquivalent) {
			extendEquivalent(hooks, (*i).baseOffset);
		}else if((*i).tag == TagType::kTagDistributed) {
			extendDistributed(hooks, (*i).baseOffset);
		}else if((*i).tag == TagType::kTagBlockedClause) {
			extendBlockedClause(hooks, (*i).baseOffset);
		}else SYS_CRITICAL("Illegal tag\n");
	}
}

}; // namespace satuzk

