
namespace satuzk {

struct VarInfo {
	static const uint8_t kFlagTouched = 1;
	static const uint8_t kFlagMarked = 2;
	static const uint8_t kFlagMinChecked = 16;
	static const uint8_t kFlagMinImplied = 32;
	static const uint8_t kFlagVarInCut = 64;
	static const uint8_t kFlagVarInMin = 128;
	uint8_t flags;
};

template<typename BaseDefs>
class LearnConfigStruct {
public:
	typedef satuzk::VariableType<BaseDefs> Variable;
	typedef satuzk::LiteralType<BaseDefs> Literal;

	typedef uint64_t LevelSignature;
	
	static LevelSignature signatureOfLevel(uint64_t level) {
		return (1 << (level % (sizeof(LevelSignature) * 8)));
	}
	
	// variable info struct for each variable
	std::vector<VarInfo> p_varInfos;
	// variables that were visited during conflict analysis
	std::vector<Variable> p_touchedVars;
	// variables that were visited at the current decision level
	std::vector<Variable> p_curlevelVars;

	// variables that are part of the non-minimized clause
	std::vector<Variable> p_cutVars;
	// variables that are part of the minimized clause
	std::vector<Literal> p_minVars;

	LevelSignature p_levelSignature;
	
	unsigned int p_minLength;

	LearnConfigStruct() : p_levelSignature(0), p_minLength(0) { }

public:
	typename std::vector<Variable>::size_type cutSize() {
		return p_cutVars.size();
	}
	typename std::vector<Variable>::iterator beginCut() {
		return p_cutVars.begin();
	}
	typename std::vector<Variable>::iterator endCut() {
		return p_cutVars.end();
	}

	typename std::vector<Literal>::size_type minSize() {
		return p_minVars.size();
	}
	Literal getMin(typename std::vector<Literal>::size_type index) {
		return p_minVars[index];
	}
	typename std::vector<Literal>::iterator beginMin() {
		return p_minVars.begin();
	}
	typename std::vector<Literal>::iterator endMin() {
		return p_minVars.end();
	}

	typename std::vector<Variable>::iterator beginCurlevel() {
		return p_curlevelVars.begin();
	}
	typename std::vector<Variable>::iterator endCurlevel() {
		return p_curlevelVars.end();
	}

	void onAllocVariable();

	// reset the learning configuration	
	void reset();

	template<typename Hooks>
	__attribute__((always_inline)) inline void visitVariable(Hooks &hooks,
			typename Hooks::Variable cause_var,
			typename Hooks::Order &link_counter);

	template<typename Hooks>
	__attribute__((always_inline)) inline void visitAntecedent(Hooks &hooks,
			typename Hooks::Variable var,
			typename Hooks::Order &link_counter);

	template<typename Hooks>
	__attribute__((always_inline)) inline void cut(Hooks &hooks,
			typename Hooks::ConflictIterator begin,
			typename Hooks::ConflictIterator end);

	// checks whether a given literal is implied by the conflict clause
	template<typename Hooks>
	__attribute__((always_inline)) inline bool minimizeLitIsImplied(Hooks &hooks,
			typename Hooks::Literal literal);

	// checks whether all causes are implied by the conflict clause
	template<typename Hooks>
	inline bool minimizeAntecedentIsImplied(Hooks &hooks,
			typename Hooks::Antecedent antecedent);

	template<typename Hooks>
	__attribute__((always_inline)) inline void minimizeRecursive(Hooks &hooks);
	
	template<typename Hooks>
	__attribute__((always_inline)) inline void minimize(Hooks &hooks);

	template<typename Hooks>
	__attribute__((always_inline)) inline void build(Hooks &hooks);
};

template<typename BaseDefs>
void LearnConfigStruct<BaseDefs>::onAllocVariable() {
	VarInfo info;
	info.flags = 0;
	p_varInfos.push_back(info);
}

template<typename BaseDefs>
void LearnConfigStruct<BaseDefs>::reset() {
	for(auto it = p_touchedVars.begin(); it != p_touchedVars.end(); ++it)
		p_varInfos[(*it).getIndex()].flags = 0;
	p_touchedVars.clear();
	p_cutVars.clear();
	p_minVars.clear();
	p_curlevelVars.clear();
	p_levelSignature = 0;
	p_minLength = 0;
}

template<typename BaseDefs>
template<typename Hooks>
void LearnConfigStruct<BaseDefs>::visitVariable(Hooks &hooks,
		typename Hooks::Variable cause_var,
		typename Hooks::Order &link_counter) {
	VarInfo &cause_info = p_varInfos[cause_var.getIndex()];
	if((cause_info.flags & VarInfo::kFlagMarked) != 0)
		return;
	//FIXME: why did we do this?
	//if(hooks.varDeclevel(cause_var) == 1)
	//	return;

	/* mark cause literals; increment activity */
	p_touchedVars.push_back(cause_var);
	cause_info.flags |= VarInfo::kFlagTouched;
	cause_info.flags |= VarInfo::kFlagMarked;
	hooks.onVarActivity(cause_var);

	if(hooks.varDeclevel(cause_var) < hooks.curDeclevel()) {
		/* add decision variables to the clause */
		cause_info.flags |= VarInfo::kFlagVarInCut;
		cause_info.flags |= VarInfo::kFlagVarInMin;
		p_minLength++;
		p_levelSignature |= signatureOfLevel(hooks.varDeclevel(cause_var));
		p_cutVars.push_back(cause_var);
	}else{
		p_curlevelVars.push_back(cause_var);
		link_counter++;
	}
}

template<typename BaseDefs>
template<typename Hooks>
void LearnConfigStruct<BaseDefs>::visitAntecedent(Hooks &hooks,
		typename Hooks::Variable var,
		typename Hooks::Order &link_counter) {
	auto antecedent = hooks.varAntecedent(var);
	if(antecedent.isDecision()) //FIXME: is this correct?
		return;
	hooks.onAntecedentActivity(antecedent);
	for(auto i = hooks.causesBegin(antecedent); i != hooks.causesEnd(antecedent); ++i)
		visitVariable(hooks, (*i).variable(), link_counter);
}

template<typename BaseDefs>
template<typename Hooks>
void LearnConfigStruct<BaseDefs>::cut(Hooks &hooks,
		typename Hooks::ConflictIterator begin,
		typename Hooks::ConflictIterator end) {
	// reserve space for the first uip
	SYS_ASSERT(SYS_ASRT_GENERAL, p_cutVars.size() == 0);
	p_cutVars.push_back(Hooks::Variable::illegalVar());

	// visit the cause of the unit assignment
	typename Hooks::Order link_counter = 0;
	for(auto i = begin; i != end; ++i) {
		SYS_ASSERT(SYS_ASRT_GENERAL, hooks.litTrue(*i));
		auto var = (*i).variable();
		visitVariable(hooks, var, link_counter);
	}
	
	typename Hooks::Order order = hooks.curOrder();
	while(true) {
		SYS_ASSERT(SYS_ASRT_GENERAL, order > 0);
		order--;

		typename Hooks::Literal literal = hooks.getOrder(order);
		typename Hooks::Variable var = literal.variable();
		VarInfo &var_info = p_varInfos[var.getIndex()];
		SYS_ASSERT(SYS_ASRT_GENERAL, hooks.varDeclevel(var) == hooks.curDeclevel());
	
		if((var_info.flags & VarInfo::kFlagMarked) == 0)
			continue;
		
		link_counter--;
		if(link_counter == 0) {
			/* add the first-uip to the clause */
			if((var_info.flags & VarInfo::kFlagTouched) == 0) {
				p_touchedVars.push_back(var);
				var_info.flags |= VarInfo::kFlagTouched;
			}
			var_info.flags |= VarInfo::kFlagVarInCut;
			var_info.flags |= VarInfo::kFlagVarInMin;
			p_minLength++;
			p_levelSignature |= signatureOfLevel(hooks.curDeclevel());
			p_cutVars[0] = var;
			break;
		}else{
			visitAntecedent(hooks, var, link_counter);
		}
	}
}

template<typename BaseDefs>
template<typename Hooks>
bool LearnConfigStruct<BaseDefs>::minimizeLitIsImplied(Hooks &hooks,
		typename Hooks::Literal literal) {
	typename Hooks::Variable var = literal.variable();
	typename Hooks::Declevel declevel = hooks.varDeclevel(var);
	if((p_levelSignature & signatureOfLevel(declevel)) == 0
			&& declevel != 1)
		return false;

	// literals that are part of the clause are obviously implied by the clause
	VarInfo &var_info = p_varInfos[var.getIndex()];
	if((var_info.flags & VarInfo::kFlagVarInCut) != 0) {
		// the conflict clause will only contain literals that are currently false
		if(hooks.litTrue(literal))
			return true;
		return false;
	}
	
	// ignore variables at decision level 1
	if(hooks.varDeclevel(var) == 1)
		return true;

	// decision variables of that are not in the clause are obviously not implied by the clause
	typename Hooks::Antecedent antecedent = hooks.varAntecedent(var);
	if(antecedent.isDecision())
		return false;
	
	// cache implication information
	if((var_info.flags & VarInfo::kFlagMinChecked) != 0)
		return (var_info.flags & VarInfo::kFlagMinImplied) != 0;

	// otherwise a literal is implied if all its causes are implied
	if(minimizeAntecedentIsImplied(hooks, antecedent)) {
		if((var_info.flags & VarInfo::kFlagTouched) == 0) {
			p_touchedVars.push_back(var);
			var_info.flags |= VarInfo::kFlagTouched;
		}
		var_info.flags |= VarInfo::kFlagMinChecked;
		var_info.flags |= VarInfo::kFlagMinImplied;
		return true;
	}else{
		if((var_info.flags & VarInfo::kFlagTouched) == 0) {
			p_touchedVars.push_back(var);
			var_info.flags |= VarInfo::kFlagTouched;
		}
		var_info.flags |= VarInfo::kFlagMinChecked;
		return false;
	}
}

template<typename BaseDefs>
template<typename Hooks>
bool LearnConfigStruct<BaseDefs>::minimizeAntecedentIsImplied(Hooks &hooks,
		typename Hooks::Antecedent antecedent) {
	for(auto i = hooks.causesBegin(antecedent); i != hooks.causesEnd(antecedent); ++i) {
		if(!minimizeLitIsImplied(hooks, *i))
			return false;
	}
	return true;
}

template<typename BaseDefs>
template<typename Hooks>
void LearnConfigStruct<BaseDefs>::minimizeRecursive(Hooks &hooks) {
	for(auto it = p_cutVars.begin(); it != p_cutVars.end(); ++it) {
		// ignore literals whose antecedent is implied by the conflict clause
		typename Hooks::Antecedent antecedent = hooks.varAntecedent(*it);
		if(antecedent.isDecision())
			continue;
		if(!minimizeAntecedentIsImplied(hooks, antecedent))
			continue;
		
		VarInfo &var_info = p_varInfos[(*it).getIndex()];
		var_info.flags &= ~VarInfo::kFlagVarInMin;
		p_minLength--;
	}
}

template<typename BaseDefs>
template<typename Hooks>
void LearnConfigStruct<BaseDefs>::minimize(Hooks &hooks) {
	minimizeRecursive(hooks);
}

template<typename BaseDefs>
template<typename Hooks>
void LearnConfigStruct<BaseDefs>::build(Hooks &hooks) {
	SYS_ASSERT(SYS_ASRT_GENERAL, p_cutVars.size() > 0);
	
	typename Hooks::Variable uip_var = p_cutVars[0];
	VarInfo &uip_info = p_varInfos[uip_var.getIndex()];
	SYS_ASSERT(SYS_ASRT_GENERAL, (uip_info.flags & VarInfo::kFlagVarInCut) != 0);
	SYS_ASSERT(SYS_ASRT_GENERAL, (uip_info.flags & VarInfo::kFlagVarInMin) != 0);
	SYS_ASSERT(SYS_ASRT_GENERAL, hooks.varDeclevel(uip_var) == hooks.curDeclevel());
	
	// find the second watched literal
	typename Hooks::Variable watch = Hooks::Variable::illegalVar();
	for(auto it = p_cutVars.begin() + 1; it != p_cutVars.end(); ++it) {
		typename Hooks::Variable var = *it;
		VarInfo &info = p_varInfos[var.getIndex()];
		SYS_ASSERT(SYS_ASRT_GENERAL, hooks.varDeclevel(var) != hooks.curDeclevel());
		if((info.flags & VarInfo::kFlagVarInMin) == 0)
			continue;
		
		SYS_ASSERT(SYS_ASRT_GENERAL, (info.flags & VarInfo::kFlagVarInCut) != 0);
		if(watch == Hooks::Variable::illegalVar() || hooks.varDeclevel(var)
				> hooks.varDeclevel(watch))
			watch = var;
	}
	
	// build an array containing all literals of the minimized clause
	// where the 0th element is the uip and the
	// 1st element is from the second decision level
	SYS_ASSERT(SYS_ASRT_GENERAL, p_minVars.size() == 0);
	p_minVars.push_back(hooks.litTrue(uip_var.oneLiteral())
			? uip_var.zeroLiteral() : uip_var.oneLiteral());
	if(watch != Hooks::Variable::illegalVar())
		p_minVars.push_back(hooks.litTrue(watch.oneLiteral())
				? watch.zeroLiteral() : watch.oneLiteral());
	for(auto it = p_cutVars.begin() + 1; it != p_cutVars.end(); ++it) {
		typename Hooks::Variable var = *it;
		VarInfo &info = p_varInfos[var.getIndex()];
		if((info.flags & VarInfo::kFlagVarInMin) == 0)
			continue;
		if(var == watch)
			continue;
		p_minVars.push_back(hooks.litTrue(var.oneLiteral())
				? var.zeroLiteral() : var.oneLiteral());
	}
}

}; // namespace satuzk

