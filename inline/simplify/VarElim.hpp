
template<typename Hooks>
bool vecdIsWorthwhile(Hooks &hooks,
		typename Hooks::Variable variable) {
	unsigned int resolvents = 0;
	typename Hooks::Literal zero_lit = variable.zeroLiteral();
	typename Hooks::Literal one_lit = variable.oneLiteral();
	unsigned int zero_count = 0;
	unsigned int one_count = 0;

	// count original clauses
	for(auto i = hooks.occurBegin(zero_lit); i != hooks.occurEnd(zero_lit); ++i) {
		if(!hooks.clauseIsPresent(*i))
			continue;
		if(hooks.clauseLength(*i) == 1)
			return false;
		zero_count++;
	}
	for(auto i = hooks.occurBegin(one_lit); i != hooks.occurEnd(one_lit); ++i) {
		if(!hooks.clauseIsPresent(*i))
			continue;
		if(hooks.clauseLength(*i) == 1)
			return false;
		one_count++;
	}

	// count resolvents
	for(auto i = hooks.occurBegin(zero_lit); i != hooks.occurEnd(zero_lit); ++i) {
		if(!hooks.clauseIsPresent(*i))
			continue;
		for(auto j = hooks.occurBegin(one_lit); j != hooks.occurEnd(one_lit); ++j) {
			if(!hooks.clauseIsPresent(*j))
				continue;
			unsigned int length;
			if(hooks.resolventLength(*i, *j, variable, length))
				continue;
			resolvents++;
			if(resolvents > zero_count + one_count)
				return false;
			if(length > 10)
				return false;
		}
	}
	return true;
}

template<typename Hooks>
void vecdEliminateVariable(Hooks &hooks,
		typename Hooks::Variable variable) {
	hooks.p_extModelConfig.pushDistributed(hooks, variable);

	std::vector<typename Hooks::Clause> added;

	typename Hooks::Literal zero_lit = variable.zeroLiteral();
	typename Hooks::Literal one_lit = variable.oneLiteral();
	
	std::vector<typename Hooks::Clause> zero_clauses;
	std::vector<typename Hooks::Clause> one_clauses;
	
	for(auto i = hooks.occurBegin(zero_lit); i != hooks.occurEnd(zero_lit); ++i) {
		if(!hooks.clauseIsPresent(*i))
			continue;
		zero_clauses.push_back(*i);
	}
	for(auto i = hooks.occurBegin(one_lit); i != hooks.occurEnd(one_lit); ++i) {
		if(!hooks.clauseIsPresent(*i))
			continue;
		one_clauses.push_back(*i);
	}
	
	// build all resolvents
	for(auto i = zero_clauses.begin(); i != zero_clauses.end(); ++i) {
		if(!hooks.clauseIsPresent(*i))
			continue;
		for(auto j = one_clauses.begin(); j != one_clauses.end(); ++j) {
			if(!hooks.clauseIsPresent(*j))
				continue;
			if(hooks.resolventTrivial(*i, *j, variable))
				continue;
			
			auto res_lits = hooks.resolventBuild(*i, *j, variable);
			typename Hooks::Clause res_clause = hooks.allocClause
					(res_lits.size(), res_lits.begin(), res_lits.end());
			if(hooks.clauseIsEssential(*i)
					|| hooks.clauseIsEssential(*j))
				hooks.clauseSetEssential(res_clause);
			hooks.p_clauseConfig.setFlagCreatedVecd(res_clause);
			added.push_back(res_clause);

			if(res_lits.size() == 1)
				hooks.stat.simp.vecdFacts++;
		}
	}
	
	// delete all clauses containing the variable
	for(auto i = zero_clauses.begin(); i != zero_clauses.end(); ++i) {
		if(hooks.clauseIsEssential(*i))
			hooks.clauseUnsetEssential(*i);
		if(hooks.clauseIsInstalled(*i))
			hooks.uninstallClause(*i);
		hooks.deleteClause(*i);
	}
	for(auto i = one_clauses.begin(); i != one_clauses.end(); ++i) {
		if(hooks.clauseIsEssential(*i))
			hooks.clauseUnsetEssential(*i);
		if(hooks.clauseIsInstalled(*i))
			hooks.uninstallClause(*i);
		hooks.deleteClause(*i);
	}
	// install new clauses
	for(auto i = added.begin(); i != added.end(); ++i)
		hooks.installClause(*i);

	// TODO: re-active this after fixing gcc 4.7 compilation errors
	for(auto i = added.begin(); i != added.end(); ++i) {
		if(!hooks.clauseIsPresent(*i))
			continue;
		if(hooks.clauseLength(*i) <= 2)
			continue;
		bceEliminateSingle(hooks, *i);
	}

	hooks.stat.simp.vecdEliminated++;
}

template<typename Hooks>
void vecdEliminateAll(Hooks &hooks) {
	std::vector<typename Hooks::Variable> queue;
	for(auto i = hooks.varsBegin(); i != hooks.varsEnd(); ++i) {
		if(!hooks.varIsPresent(*i))
			continue;
		queue.push_back(*i);
	}

	for(auto i = queue.begin(); i != queue.end(); ++i) {
		if(hooks.varIsLocked(*i))
			continue;
		if(!vecdIsWorthwhile(hooks, *i))
			continue;
		vecdEliminateVariable(hooks, *i);
		
		hooks.deleteVar(*i);
	}

	hooks.checkClauseGarbage();
}

