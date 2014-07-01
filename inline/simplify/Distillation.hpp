
template<typename Hooks>
class DistLitActivityLt {
public:
	DistLitActivityLt(Hooks &hooks) : p_hooks(hooks) { }

	bool operator() (typename Hooks::Literal literal1,
			typename Hooks::Literal literal2) {
		auto var1 = literal1.variable();
		auto var2 = literal2.variable();
		if(p_hooks.varGetActivity(var1) < p_hooks.varGetActivity(var2))
			return true;
		return false;
	}

private:
	Hooks &p_hooks;
};

template<typename Hooks>
class DistClauseActivityLt {
public:
	DistClauseActivityLt(Hooks &hooks) : p_hooks(hooks) { }
	
	bool operator() (typename Hooks::Clause clause1,
			typename Hooks::Clause clause2) {
		return p_hooks.clause_get_activity(clause1)
				> p_hooks.clause_get_activity(clause2);
	}

private:
	Hooks &p_hooks;
};

template<typename Hooks>
class DistClausesLt {
public:
	DistClausesLt(Hooks &hooks) : p_hooks(hooks) { }

	float weight(typename Hooks::Clause clause) {
		float w = 0;
		for(auto i = p_hooks.clauseBegin(clause); i != p_hooks.clauseEnd(clause); ++i)
			w += p_hooks.varGetActivity((*i).variable());
		w /= p_hooks.clauseLength(clause);
		return w;
	}

	bool operator() (typename Hooks::Clause clause1,
			typename Hooks::Clause clause2) {
		return weight(clause1) > weight(clause2);
	}

private:
	Hooks &p_hooks;
};

enum class DistResult {
	kNone, kAtSolution, kClauseFixed, kMinimal, kDistilled
};

template<typename Defs>
struct DistConfig {
	std::vector<bool> marked;
	std::vector<typename Defs::Literal> visited;
	std::vector<typename Defs::Literal> reasons;
	std::vector<typename Defs::Literal> newLiterals;
	bool redundant;

	DistConfig() : redundant(false) {
	}
	
	void onAllocVariable() {
		marked.push_back(false);
		marked.push_back(false);
	}

	template<typename Hooks>
	void conflict(Hooks &hooks, typename Defs::Clause clause);

	template<typename Hooks>
	void assertTrue(Hooks &hooks, typename Defs::Clause clause,
			typename Defs::Literal asserted);

	template<typename Hooks>
	void assertFalse(Hooks &hooks, typename Defs::Clause clause,
			typename Defs::Literal asserted);

	template<typename Hooks>
	DistResult distill(Hooks &hooks, typename Defs::Clause clause,
			typename Defs::Literal to_assert);

	void reset() {
		redundant = false;
		newLiterals.clear();
	}
};

template<typename Hooks>
void distVisit(Hooks &hooks,
		typename Hooks::Literal literal,
		std::vector<bool> &marked,
		std::vector<typename Hooks::Literal> &visited,
		std::vector<typename Hooks::Literal> &reasons) {
	SYS_ASSERT(SYS_ASRT_GENERAL, !marked[literal.getIndex()]);
	SYS_ASSERT(SYS_ASRT_GENERAL, hooks.litTrue(literal));
	marked[literal.getIndex()] = true;
	visited.push_back(literal);

	auto var = literal.variable();
	auto antecedent = hooks.varAntecedent(var);
	if(antecedent == Hooks::Antecedent::makeDecision()) {
		if(!(hooks.varIsFixed(var) && !hooks.varIsLocked(var)))
			reasons.push_back(literal);
	}else{
		for(auto i = hooks.causesBegin(antecedent); i != hooks.causesEnd(antecedent); ++i) {
			if(marked[(*i).getIndex()])
				continue;
			distVisit(hooks, *i, marked, visited, reasons);
		}
	}
}

template<typename Hooks>
void distCleanup(Hooks &hooks,
		std::vector<bool> &marked,
		std::vector<typename Hooks::Literal> &visited,
		std::vector<typename Hooks::Literal> &reasons) {
	for(auto i = visited.begin(); i != visited.end(); ++i)
		marked[(*i).getIndex()] = false;
	visited.clear();
	reasons.clear();
}

template<typename Defs>
template<typename Hooks>
void DistConfig<Defs>::conflict(Hooks &hooks, typename Defs::Clause clause) {
	for(auto i = hooks.conflictBegin(); i != hooks.conflictEnd(); ++i) {
		if(marked[(*i).getIndex()])
			continue;
		distVisit(hooks, *i, marked, visited, reasons);
	}

	bool subsumes = true;
	for(auto j = reasons.begin(); j != reasons.end(); ++j) {
		auto inv = (*j).inverse();
		if(std::find(hooks.clauseBegin(clause), hooks.clauseEnd(clause), inv)
				== hooks.clauseEnd(clause))
			subsumes = false;
		newLiterals.push_back(inv);
	}
	SYS_ASSERT(SYS_ASRT_GENERAL, newLiterals.size() > 0);
	
	if(subsumes && newLiterals.size() < hooks.clauseLength(clause)) {
		redundant = true;
		hooks.stat.simp.distConflicts++;
		unsigned int removed_lits = hooks.clauseLength(clause)
				- newLiterals.size();
		hooks.stat.simp.distConflictsRemoved += removed_lits;
	}
	distCleanup(hooks, marked, visited, reasons);
}

// the asserted literal is always true in this function
template<typename Defs>
template<typename Hooks>
void DistConfig<Defs>::assertTrue(Hooks &hooks,
		typename Defs::Clause clause,
		typename Defs::Literal asserted) {
	distVisit(hooks, asserted, marked, visited, reasons);

	// the new clause is simply the implication (reasons -> asserted)
	bool subsumes = true;
	for(auto j = reasons.begin(); j != reasons.end(); ++j) {
		auto inv = (*j).inverse();
		if(std::find(hooks.clauseBegin(clause), hooks.clauseEnd(clause), inv)
				== hooks.clauseEnd(clause))
			subsumes = false;
		newLiterals.push_back(inv);
	}
	newLiterals.push_back(asserted);

	if(subsumes && newLiterals.size() < hooks.clauseLength(clause)) {
		redundant = true;
		hooks.stat.simp.distAsserts++;
		unsigned int removed_lits = hooks.clauseLength(clause)
				- newLiterals.size();
		hooks.stat.simp.distAssertsRemoved += removed_lits;
	}
	distCleanup(hooks, marked, visited, reasons);
}

// the asserted literal is always false in this function
template<typename Defs>
template<typename Hooks>
void DistConfig<Defs>::assertFalse(Hooks &hooks,
		typename Defs::Clause clause,
		typename Defs::Literal asserted) {
	distVisit(hooks, asserted.inverse(), marked, visited, reasons);
	
	/* the new clause is constructed by resolving the old clause
	 * with the implication (reasons -> not asserted) */
	bool subsumes = true;
	for(auto j = reasons.begin(); j != reasons.end(); ++j) {
		auto inv = (*j).inverse();
		if(std::find(hooks.clauseBegin(clause), hooks.clauseEnd(clause), inv)
				== hooks.clauseEnd(clause))
			subsumes = false;
		newLiterals.push_back(inv);
	}
	for(auto j = hooks.clauseBegin(clause); j != hooks.clauseEnd(clause); ++j) {
		if(*j == asserted)
			continue;
		newLiterals.push_back(*j);
	}
	SYS_ASSERT(SYS_ASRT_GENERAL, newLiterals.size() > 0);
	
	if(subsumes && newLiterals.size() < hooks.clauseLength(clause)) {
		redundant = true;
		hooks.stat.simp.distSelfSubs++;
		unsigned int removed_lits = hooks.clauseLength(clause)
				- newLiterals.size();
		hooks.stat.simp.distSelfSubsRemoved += removed_lits;
	}
	distCleanup(hooks, marked, visited, reasons);
}

template<typename Defs>
template<typename Hooks>
DistResult DistConfig<Defs>::distill(Hooks &hooks,
		typename Defs::Clause clause, typename Defs::Literal to_assert) {
	// return if the formula is already solved
	hooks.reset();
	hooks.start();
	if(hooks.atConflict()) {
		SYS_ASSERT(SYS_ASRT_GENERAL, !hooks.isResolveable());
		return DistResult::kAtSolution;
	}
	
	std::vector<typename Hooks::Literal> literals;
	for(auto i = hooks.clauseBegin(clause); i != hooks.clauseEnd(clause); ++i) {
		/* if the clause is unit under the current assumptions
		 * we wont be able to deduce anything valuable */
		if(hooks.litTrue(*i))
			return DistResult::kClauseFixed;
		literals.push_back(*i);
	}
	DistLitActivityLt<Hooks> order(hooks);
	std::sort(literals.begin(), literals.end(), order);

	for(auto i = literals.begin(); i != literals.end(); ++i) {
		auto var = (*i).variable();
		if(hooks.litTrue(*i)) {
			if(hooks.varAntecedent(var) != Hooks::Antecedent::makeClause(clause))
				assertTrue(hooks, clause, *i);
			break;
		}else if(hooks.litFalse(*i)) {
			assertFalse(hooks, clause, *i);
			break;
		}
			
		// assign the i-th literal of the clause
		hooks.pushLevel();
		hooks.pushAssign((*i).inverse(), Hooks::Antecedent::makeDecision());
		hooks.propagate();
		if(hooks.atConflict())
			break;
	}

	if(hooks.atConflict())
		conflict(hooks, clause);
	
	while(hooks.atConflict()) {
		if(!hooks.isResolveable())
			break;
		hooks.resolveConflict();
		hooks.propagate();
	}
	
	hooks.stat.simp.distChecks++;

	if(!redundant)
		return DistResult::kMinimal;
	return DistResult::kDistilled;
}

