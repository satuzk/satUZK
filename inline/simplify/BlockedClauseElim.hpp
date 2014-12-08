
// checks whether the given literal blocks the given clause.
// the given literal must be in the clause.
template<typename Hooks>
bool bceBlocks(Hooks &hooks, typename Hooks::Clause clause,
		typename Hooks::Literal literal) {
	if(hooks.litFalse(literal))
		return false;/*FIXME: what does this check do? */

	typename Hooks::Literal inverse = literal.inverse();
	for(auto i = hooks.occurBegin(inverse); i != hooks.occurEnd(inverse); ++i) {
		if(!hooks.clauseIsPresent(clause))
			continue;
		if(!hooks.resolventTrivial(clause, *i, literal.variable()))
			return false;
	}
	return true;
}

// checks whether the given clause is a blocked clause
template<typename Hooks>
bool bceBlocked(Hooks &hooks, typename Hooks::Clause clause,
		typename Hooks::Literal &blocking) {
	for(auto i = hooks.clauseBegin(clause); i != hooks.clauseEnd(clause); ++i) {
		if(hooks.varIsLocked((*i).variable()))
			continue;
		if(bceBlocks(hooks, clause, *i)) {
			blocking = *i;
			return true;
		}
	}
	return false;
}

template<typename Hooks>
bool bceEliminateSingle(Hooks &hooks,
		typename Hooks::Clause clause) {
	typename Hooks::Literal blocking = Hooks::Literal::illegalLit();
	if(!bceBlocked(hooks, clause, blocking))
		return false;
		
	hooks.p_extModelConfig.pushBlockedClause(hooks, clause, blocking);

	if(hooks.clauseIsEssential(clause))
		hooks.clauseUnsetEssential(clause);
	if(hooks.clauseIsInstalled(clause))
		hooks.uninstallClause(clause);
	hooks.deleteClause(clause);

	hooks.stat.simp.bceEliminated++;
	return true;
}

// performs blocked clause elimination on all clauses
template<typename Hooks>
void bceEliminateAll(Hooks &hooks) {
	std::vector<typename Hooks::Clause> queue;
	for(auto i = hooks.clausesBegin(); i != hooks.clausesEnd(); ++i) {
		if(!hooks.clauseIsPresent(*i))
			continue;
		if(hooks.clauseLength(*i) <= 2)
			continue;
		queue.push_back(*i);
	}

	for(auto i = queue.begin(); i != queue.end(); ++i)
		bceEliminateSingle(hooks, *i);

	hooks.checkClauseGarbage();
}

