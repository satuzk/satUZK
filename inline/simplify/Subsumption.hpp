
static const uint64_t m1 = 0x5555555555555555LL;
static const uint64_t m2 = 0x3333333333333333LL;
static const uint64_t m4 = 0x0f0f0f0f0f0f0f0fLL;
static const uint64_t m8 = 0x00ff00ff00ff00ffLL;
static const uint64_t m16 = 0x0000ffff0000ffffLL;
static const uint64_t m32 = 0x00000000ffffffffLL;
static const uint64_t hff = 0xffffffffffffffffLL;
static const uint64_t h01 = 0x0101010101010101LL;

inline uint64_t popcount(uint64_t x) {
    x = (x & m1) + ((x >> 1) & m1);
    x = (x & m2) + ((x >> 2) & m2);
    x = (x & m4) + ((x >> 4) & m4);
    x = (x & m8) + ((x >> 8) & m8);
    x = (x & m16) + ((x >> 16) & m16);
    x = (x & m32) + ((x >> 32) & m32);
    return x;
}

// checks whether the resolvent of both clauses subsumes the
// second clause. the resolved variable must be in both clauses, with opposite polarity.
template<typename Hooks>
bool resolventSubsumes(Hooks &hooks,
		typename Hooks::Clause clause,
		typename Hooks::Clause subsumed,
		typename Hooks::Variable resolved) {
	if(hooks.clauseLength(clause) > hooks.clauseLength(subsumed))
		return false;
	
	auto clause_sig = hooks.clauseGetSignature(clause);
	auto subsumed_sig = hooks.clauseGetSignature(subsumed);
	typename Hooks::ClauseSignature diff = (~clause_sig) & subsumed_sig;
	if(popcount(diff) >= 2) {
		hooks.stat.simp.selfsubShortcuts++;
		return false;
	}

	hooks.stat.simp.selfsubSubChecks++;
	for(auto i = hooks.clauseBegin(clause); i != hooks.clauseEnd(clause); ++i) {
		typename Hooks::Variable var = (*i).variable();
		if(var == resolved)	
			continue;
		auto it = std::find(hooks.clauseBegin(subsumed), hooks.clauseEnd(subsumed), *i);
		if(it == hooks.clauseEnd(subsumed))
			return false;
	}
	return true;
}

// tests if the first clause self-subsumes the second
// and returns the variable that can be resolved
template<typename Hooks>
bool clauseSelfsubs(Hooks &hooks,
		typename Hooks::Clause clause,
		typename Hooks::Clause subsumed,
		typename Hooks::Variable &resolved) {
	resolved = Hooks::Variable::illegalVar();
	
	if(hooks.clauseLength(clause) > hooks.clauseLength(subsumed))
		return false;
	
	auto clause_sig = hooks.clauseGetSignature(clause);
	auto subsumed_sig = hooks.clauseGetSignature(subsumed);
	typename Hooks::ClauseSignature diff = (~clause_sig) & subsumed_sig;
	if(popcount(diff) >= 2) {
		hooks.stat.simp.selfsubShortcuts++;
		return false;
	}

	hooks.stat.simp.selfsubResChecks++;
	for(auto i = hooks.clauseBegin(clause); i != hooks.clauseEnd(clause); ++i) {
		// test if the variable is in the subsumed clause
		typename Hooks::Variable var = (*i).variable();
		int subsumed_pol = hooks.clausePolarity(subsumed, var);
		if(subsumed_pol == 0)
			return false;
		
		// test if both polarities are equal
		int clause_pol = (*i).polarity();
		if(clause_pol == subsumed_pol)
			continue;
		
		// test if there are two literals of opposite polarities
		if(resolved != Hooks::Variable::illegalVar())
			return false;
		resolved = var;
	}
	return true;
}

// determine the literal with the shortest occurrence list
template<typename Hooks>
typename Hooks::Literal selfsubShortestOcclist(Hooks &hooks,
		typename Hooks::Clause clause) {
	unsigned int best_length = 0;
	typename Hooks::Literal best_literal = Hooks::Literal::illegalLit();
	for(auto i = hooks.clauseBegin(clause); i != hooks.clauseEnd(clause); ++i) {
		typename Hooks::Literal inverse = (*i).inverse();
		unsigned int length = hooks.occurSize(*i) + hooks.occurSize(inverse);
		if(best_literal == Hooks::Literal::illegalLit() || length < best_length) {
			best_length = length;
			best_literal = *i;
		}
	}
	return best_literal;
}

// checks whether the given clause subsumes any other one
template<typename Hooks>
void selfsubBackward(Hooks &hooks,
		typename Hooks::Clause clause,
		typename Hooks::Literal sub_literal) {
	SYS_ASSERT(SYS_ASRT_GENERAL, hooks.clauseIsPresent(clause));
	typename Hooks::Literal sub_inverse = sub_literal.inverse();

	std::vector<typename Hooks::Clause> queue;	
	for(auto j = hooks.occurBegin(sub_inverse); j != hooks.occurEnd(sub_inverse); ++j)
		queue.push_back(*j);

	// check the clauses containing the inverse:
	// we only have to check if the resolvent subsumes the second clause
	typename Hooks::Variable sub_var = sub_literal.variable();
	for(auto j = queue.begin(); j != queue.end(); ++j) {
		if(!hooks.clauseIsPresent(*j))
			continue;
		if(!resolventSubsumes(hooks, clause, *j, sub_var))
			continue;
		
		// build the resolvent and delete the subsumed clause
		auto res_lits = hooks.resolventBuild(*j, clause, sub_var);
		
		typename Hooks::Clause res_clause = hooks.allocClause
				(res_lits.size(), res_lits.begin(), res_lits.end());
		if(hooks.clauseIsEssential(*j))
			hooks.clauseSetEssential(res_clause);
			
		hooks.installClause(res_clause);
		
		if(hooks.clauseIsEssential(*j))
			hooks.clauseUnsetEssential(*j);
		if(hooks.clauseIsInstalled(*j))
			hooks.uninstallClause(*j);
		hooks.deleteClause(*j);
		
		if(res_lits.size() == 1)
			hooks.stat.simp.selfsubFacts++;
		hooks.stat.simp.selfsubClauses++;
	}
	
	queue.clear();
	for(auto j = hooks.occurBegin(sub_literal); j != hooks.occurEnd(sub_literal); ++j)
		queue.push_back(*j);
	
	// check the clauses containing the same literal as our clause:
	// we have to do a full subsumption/self-subsumption check
	for(auto j = queue.begin(); j != queue.end(); ++j) {
		if(*j == clause)
			continue;
		if(!hooks.clauseIsPresent(*j))
			continue;
		
		typename Hooks::Variable res_var = Hooks::Variable::illegalVar();
		if(!clauseSelfsubs(hooks, clause, *j, res_var))
			continue;
		
		if(res_var == Hooks::Variable::illegalVar()) {
			// regular subsumption: delete the subsumed clause
			if(hooks.clauseIsEssential(*j) && !hooks.clauseIsEssential(clause))
				hooks.clauseSetEssential(clause);
			
			if(hooks.clauseIsEssential(*j))
				hooks.clauseUnsetEssential(*j);
			if(hooks.clauseIsInstalled(*j))
				hooks.uninstallClause(*j);
			hooks.deleteClause(*j);
			
			hooks.stat.simp.subsumed_clauses++;
		}else{
			// self-subsumption: build the resolvent and delete the subsumed clause
			auto res_lits = hooks.resolventBuild(*j, clause, res_var);
			
			typename Hooks::Clause res_clause = hooks.allocClause
					(res_lits.size(), res_lits.begin(), res_lits.end());
			if(hooks.clauseIsEssential(*j))
				hooks.clauseSetEssential(res_clause);
			
			hooks.installClause(res_clause);
			
			if(hooks.clauseIsEssential(*j))
				hooks.clauseUnsetEssential(*j);
			if(hooks.clauseIsInstalled(*j))
				hooks.uninstallClause(*j);
			hooks.deleteClause(*j);
			
			if(res_lits.size() == 1)
				hooks.stat.simp.selfsubFacts++;
			hooks.stat.simp.selfsubClauses++;
		}
	}
}

// applies forward subsumption to all clauses
template<typename Hooks>
void selfsubEliminateAll(Hooks &hooks) {
	// we need a queue here because the occlists can change during the algorithm
	std::vector<typename Hooks::Clause> queue;
	for(auto i = hooks.clausesBegin(); i != hooks.clausesEnd(); ++i)
		queue.push_back(*i);
	
	for(auto j = queue.begin(); j != queue.end(); ++j) {
		if(!hooks.clauseIsPresent(*j))
			continue;
		if(hooks.clauseLength(*j) == 1)
			continue;
		
		typename Hooks::Literal literal = selfsubShortestOcclist(hooks, *j);
		selfsubBackward(hooks, *j, literal);
	}
	
	hooks.checkClauseGarbage();
}

