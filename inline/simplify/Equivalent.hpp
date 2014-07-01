
template<typename Hooks>
void equivReplaceClause(Hooks &hooks,
		typename Hooks::Clause clause) {
	int changes = 0;
	std::vector<typename Hooks::Literal> new_literals;
	for(auto j = hooks.clauseBegin(clause); j != hooks.clauseEnd(clause); ++j) {
		auto equiv = hooks.litEquivalent(*j);
		auto inverse = hooks.litEquivalent((*j).inverse());
		SYS_ASSERT(SYS_ASRT_GENERAL, inverse == equiv.inverse());
		
		if(equiv != *j)
			changes++;		

		// TODO: unfortunately this has quadratic runtime
		// check if the literal became redundant
		if(std::find(new_literals.begin(), new_literals.end(), equiv)
				!= new_literals.end())
			continue;
		
		// check if the new clause is a tautology
		if(std::find(new_literals.begin(), new_literals.end(), inverse)
				!= new_literals.end()) {
			if(hooks.clauseIsEssential(clause))
				hooks.clauseUnsetEssential(clause);
			if(hooks.clauseIsInstalled(clause))
				hooks.uninstallClause(clause);
			hooks.deleteClause(clause);
			return;
		}
		
		new_literals.push_back(equiv);
	}
	if(changes == 0)
		return;
	
	auto new_clause = hooks.allocClause(new_literals.size(),
			new_literals.begin(), new_literals.end());
	if(hooks.clauseIsEssential(clause))
		hooks.clauseSetEssential(new_clause);
	hooks.installClause(new_clause);
	
	if(hooks.clauseIsEssential(clause))
		hooks.clauseUnsetEssential(clause);
	if(hooks.clauseIsInstalled(clause))
		hooks.uninstallClause(clause);
	hooks.deleteClause(clause);
}

template<typename Hooks>
void equivReplaceAll(Hooks &hooks) {
	for(auto it = hooks.varsBegin(); it != hooks.varsEnd(); ++it) {
		if(!hooks.varIsPresent(*it))
			continue;
		
		auto l0 = (*it).zeroLiteral();
		auto l1 = (*it).oneLiteral();
		auto root0 = hooks.litEquivalent(l0);
		auto root1 = hooks.litEquivalent(l1);

		SYS_ASSERT(SYS_ASRT_GENERAL, root0 != root1);
		SYS_ASSERT(SYS_ASRT_GENERAL, ((root0 != l0) && (root1 != l1))
				|| ((root0 == l0) && (root1 == l1)));
		SYS_ASSERT(SYS_ASRT_GENERAL, root0 == root1.inverse());

		if(root0 == l0)
			continue;

		SYS_ASSERT(SYS_ASRT_GENERAL, hooks.litState(l0) == hooks.litState(root0));
		hooks.p_extModelConfig.pushEquivalent(hooks, *it, root0);
		hooks.deleteVar(*it);
	}

	std::vector<typename Hooks::Clause> queue;
	for(auto i = hooks.clausesBegin(); i != hooks.clausesEnd(); ++i) {
		if(!hooks.clauseIsPresent(*i))
			continue;
		queue.push_back(*i);
	}
	for(auto i = queue.begin(); i != queue.end(); ++i)
		equivReplaceClause(hooks, *i);
	
	hooks.checkClauseGarbage();
}

