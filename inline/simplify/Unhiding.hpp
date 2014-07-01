
struct UnhideRunStats {
	unsigned int equivalentLiterals;
	unsigned int failedLiterals;
	unsigned int transitiveEdges;
	unsigned int hleLiterals;
	unsigned int hteClauses;
	
	UnhideRunStats() : equivalentLiterals(0),
			failedLiterals(0), transitiveEdges(0),
			hleLiterals(0), hteClauses(0) { }
};

template<typename Hooks>
void unhideStamp(Hooks &hooks,
		typename Hooks::Literal literal,
		typename Hooks::Literal this_root,
		unsigned int depth,
		unsigned int &next_timestamp,
		std::vector<unsigned int> &observed,
		std::vector<unsigned int> &preorder,
		std::vector<unsigned int> &postorder,
		std::vector<typename Hooks::Literal> &root,
		std::vector<typename Hooks::Literal> &parent,
		std::vector<typename Hooks::Literal> &stack,
		std::vector<typename Hooks::Clause> &redundant,
		UnhideRunStats &run_stats) {
	SYS_ASSERT(SYS_ASRT_GENERAL, hooks.varIsPresent(literal.variable()));
	SYS_ASSERT(SYS_ASRT_GENERAL, preorder[literal.getIndex()] == 0);
	next_timestamp++;
	preorder[literal.getIndex()] = next_timestamp;
	observed[literal.getIndex()] = next_timestamp;
	root[literal.getIndex()] = this_root;
	stack.push_back(literal);

	bool scc_root = true;
	for(auto i = hooks.watchBegin(literal); i != hooks.watchEnd(literal); ++i) {
		if(!(*i).isBinary())
			continue;

		auto dest = (*i).binaryGetImplied();
		auto inverse = dest.inverse();

		// check if we found a transitive edge
		if(preorder[literal.getIndex()] < observed[dest.getIndex()]) {
			// mark the edges (and reverse edge) as transitive
			redundant.push_back((*i).binaryGetClause());
			
			hooks.stat.simp.unhideTransitiveEdges++;
			run_stats.transitiveEdges++;
			continue;
		}

		// if a literal and its inverse are both observed from the same root
		// all common ancestors of the literal are failed.
		if(preorder[this_root.getIndex()] <= observed[inverse.getIndex()]) {
			// find the last common ancestor
			typename Hooks::Literal failed = literal;
			while(preorder[failed.getIndex()] > observed[inverse.getIndex()])
				failed = parent[failed.getIndex()];
			
			// install a clause that forbids this literal
			std::array<typename Hooks::Literal, 1> unit_lits;
			unit_lits[0] = failed.inverse();
			typename Hooks::Clause unit_clause = hooks.allocClause(1,
					unit_lits.begin(), unit_lits.end());
			hooks.clauseSetEssential(unit_clause);
			hooks.installClause(unit_clause);

			hooks.stat.simp.unhideFailedLiterals++;
			run_stats.failedLiterals++;

			if(preorder[inverse.getIndex()] != 0 && postorder[inverse.getIndex()] == 0)
				continue;
		}
		
		if(preorder[dest.getIndex()] == 0) {
			parent[dest.getIndex()] = literal;
			unhideStamp(hooks, dest, this_root, depth + 1, next_timestamp,
					observed, preorder, postorder, root, parent, stack, redundant, run_stats);
		}
		
		// check if we found a backward edge
		if(postorder[dest.getIndex()] == 0 && preorder[dest.getIndex()] < preorder[literal.getIndex()]) {
			preorder[literal.getIndex()] = preorder[dest.getIndex()];
			scc_root = false;
		}
		observed[dest.getIndex()] = next_timestamp;
	}

	// there were no backward edges:
	// this literal is the head of a strongly connected component
	if(scc_root) {
		next_timestamp++;
		typename Hooks::Literal equivalent;
		do {
			equivalent = stack.back();
			stack.pop_back();
			preorder[equivalent.getIndex()] = preorder[literal.getIndex()];
			postorder[equivalent.getIndex()] = next_timestamp;
			
			/* TODO: make sure this substitution is legal */
			auto root1 = hooks.litEquivalent(literal);
			auto root2 = hooks.litEquivalent(equivalent);
			if(root1 != root2) {
				hooks.litJoin(equivalent, literal);
				hooks.litJoin(equivalent.inverse(), literal.inverse());
				hooks.stat.simp.sccVariables++;
				run_stats.equivalentLiterals++;
			}
		} while(equivalent != literal);
	}
}

template<typename Hooks>
void unhideHle(Hooks &hooks,
		typename Hooks::Clause clause,
		std::vector<unsigned int> &discovery,
		std::vector<unsigned int> &finish,
		std::vector<typename Hooks::Literal> &s_plus,
		std::vector<typename Hooks::Literal> &s_minus,
		UnhideRunStats &run_stats) {
	unsigned int num_marked = 0;
	
	unsigned int finished = 0;
	for(auto i = s_plus.rbegin(); i != s_plus.rend(); ++i) {
		if(hooks.p_varConfig.getLitFlagMarked(*i))
			continue;
		if(finished == 0) {
			finished = finish[(*i).getIndex()];
		}else if(finish[(*i).getIndex()] > finished) {
			hooks.p_varConfig.setLitFlagMarked(*i);
			num_marked++;
		}else finished = finish[(*i).getIndex()];
	}

	finished = 0;
	for(auto i = s_minus.begin(); i != s_minus.end(); ++i) {
		typename Hooks::Literal inv = (*i).inverse();
		if(hooks.p_varConfig.getLitFlagMarked(inv))
			continue;
		if(finished == 0) {
			finished = finish[(*i).getIndex()];
		}else if(finish[(*i).getIndex()] < finished) {
			hooks.p_varConfig.setLitFlagMarked(inv);
			num_marked++;
		}else finished = finish[(*i).getIndex()];
	}

	if(num_marked == 0)
		return;

	std::vector<typename Hooks::Literal> new_lits;
	for(auto i = hooks.clauseBegin(clause); i != hooks.clauseEnd(clause); ++i) {
		bool marked = hooks.p_varConfig.getLitFlagMarked(*i);
		hooks.p_varConfig.clearLitFlagMarked(*i);
		if(marked)
			continue;
		new_lits.push_back(*i);
	}

	auto new_clause = hooks.allocClause(new_lits.size(),
			new_lits.begin(), new_lits.end());
	if(hooks.clauseIsEssential(clause))
		hooks.clauseSetEssential(new_clause);
	hooks.clauseSetLbd(new_clause, hooks.clauseGetLbd(clause));
	hooks.clauseSetActivity(new_clause, hooks.clauseGetActivity(clause));
	hooks.installClause(new_clause);
	
	hooks.stat.simp.unhideHleLiterals += num_marked;
	run_stats.hleLiterals += num_marked;

	if(hooks.clauseIsEssential(clause))
		hooks.clauseUnsetEssential(clause);
	if(hooks.clauseIsInstalled(clause))
		hooks.uninstallClause(clause);
	hooks.deleteClause(clause);
}

template<typename Hooks>
bool unhideHte(Hooks &hooks,
		typename Hooks::Clause clause,
		std::vector<unsigned int> &discovery,
		std::vector<unsigned int> &finish,
		std::vector<typename Hooks::Literal> &s_plus,
		std::vector<typename Hooks::Literal> &s_minus) {
	auto l_pos = s_plus.begin();
	auto l_neg = s_minus.begin();
	while(true) {
		if(discovery[(*l_neg).getIndex()] > discovery[(*l_pos).getIndex()]) {
			l_pos++;
			if(l_pos == s_plus.end())
				return false;
		}else if(finish[(*l_neg).getIndex()] < finish[(*l_pos).getIndex()]
				|| hooks.clauseLength(clause) == 2) {
			l_neg++;
			if(l_neg == s_minus.end())
				return false;
		}else return true;
	}
}

template<typename Hooks>
class UnhidingPreoderLt {
public:
	UnhidingPreoderLt(std::vector<unsigned int> &preorder)
			: p_preorder(preorder) { }

	bool operator() (typename Hooks::Literal literal1,
			typename Hooks::Literal literal2) {
		return p_preorder[literal1.getIndex()] < p_preorder[literal2.getIndex()];
	}
private:
	std::vector<unsigned int> &p_preorder;
};

template<typename Hooks>
void unhideEliminateAll(Hooks &hooks,
		bool perform_hte, UnhideRunStats &run_stats) {
	std::vector<unsigned int> observed;
	std::vector<unsigned int> preorder;
	std::vector<unsigned int> postorder;
	std::vector<typename Hooks::Literal> root;
	std::vector<typename Hooks::Literal> parent;
	std::vector<typename Hooks::Literal> stack;

	unsigned int lit_count = hooks.numVariables() * 2;
	observed.resize(lit_count, 0);
	preorder.resize(lit_count, 0);
	postorder.resize(lit_count, 0);
	root.resize(lit_count, Hooks::Literal::illegalLit());
	parent.resize(lit_count, Hooks::Literal::illegalLit());

	unsigned int next_timestamp = 1;

	std::vector<typename Hooks::Variable> var_queue;
	for(auto it = hooks.varsBegin(); it != hooks.varsEnd(); ++it) {
		if(!hooks.varIsPresent(*it))
			continue;
		var_queue.push_back(*it);
	}
	std::shuffle(var_queue.begin(), var_queue.end(), hooks.p_rndEngine);

	std::vector<typename Hooks::Clause> redundant;

	for(auto i = var_queue.begin(); i != var_queue.end(); ++i) {
		auto zero_lit = (*i).zeroLiteral();
		auto one_lit = (*i).oneLiteral();
		if(preorder[zero_lit.getIndex()] == 0)
			unhideStamp(hooks, zero_lit, zero_lit, 0, next_timestamp,
					observed, preorder, postorder, root, parent, stack, redundant, run_stats);
		if(preorder[one_lit.getIndex()] == 0)
			unhideStamp(hooks, one_lit, one_lit, 0, next_timestamp,
					observed, preorder, postorder, root, parent, stack, redundant, run_stats);
	}

	for(auto i = redundant.begin(); i != redundant.end(); ++i) {
		// NOTE: the redundant vector may contain the same clause twice
		if(!hooks.clauseIsPresent(*i))
			continue;
		
		if(hooks.clauseIsEssential(*i))
			hooks.clauseUnsetEssential(*i);
		if(hooks.clauseIsInstalled(*i))
			hooks.uninstallClause(*i);
		hooks.deleteClause(*i);
	}
	
	// substitute equivalent literals
	equivReplaceAll(hooks);

	// perform hle and hte
	UnhidingPreoderLt<Hooks> comparator(preorder);
	std::vector<typename Hooks::Literal> s_plus;
	std::vector<typename Hooks::Literal> s_minus;
	
	std::vector<typename Hooks::Clause> queue;
	for(auto i = hooks.clausesBegin(); i != hooks.clausesEnd(); ++i) {
		if(!hooks.clauseIsPresent(*i))
			continue;
		queue.push_back(*i);
	}
	for(auto i = queue.begin(); i != queue.end(); ++i) {
		s_plus.clear();
		s_minus.clear();
		for(auto j = hooks.clauseBegin(*i); j != hooks.clauseEnd(*i); ++j) {
			s_plus.push_back(*j);
			s_minus.push_back((*j).inverse());
		}
		std::sort(s_plus.begin(), s_plus.end(), comparator);
		std::sort(s_minus.begin(), s_minus.end(), comparator);
		
		if(perform_hte && unhideHte(hooks, *i, preorder, postorder, s_plus, s_minus)) {
			if(hooks.clauseIsEssential(*i))
				hooks.clauseUnsetEssential(*i);
			if(hooks.clauseIsInstalled(*i))
				hooks.uninstallClause(*i);
			hooks.deleteClause(*i);
			
			hooks.stat.simp.unhideHteClauses++;
			run_stats.hteClauses++;
		}else unhideHle(hooks, *i, preorder, postorder,
				s_plus, s_minus, run_stats);
	}
	
	hooks.checkClauseGarbage();
}

