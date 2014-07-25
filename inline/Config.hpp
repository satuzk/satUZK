
namespace satuzk {

/* --------------------- VARIABLE MANAGEMENT FUNCTIONS --------------------- */

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::varReserve(typename BaseDefs::LiteralIndex count) {
	p_varConfig.reserveVars(count);
}

template<typename BaseDefs, typename Hooks>
typename Config<BaseDefs, Hooks>::Variable Config<BaseDefs, Hooks>::varAlloc() {
	Variable var = p_varConfig.allocVar();

	p_learnConfig.onAllocVariable();
	p_vsidsConfig.onAllocVariable(var);
	return var;
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::deleteVar(Config<BaseDefs, Hooks>::Variable var) {
	p_varConfig.deleteVar(var);
};

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::randomizeVsids() {
	std::uniform_real_distribution<Activity> activity_dist(0, 5);
	std::bernoulli_distribution phase_dist(0.5);
	for(auto it = varsBegin(); it != varsEnd(); ++it) {
		p_vsidsConfig.incActivity(*it, activity_dist(p_rndEngine));

		if(phase_dist(p_rndEngine))
			p_varConfig.setVarFlagSaved(*it);
	}
}

template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::varIsPresent(Config<BaseDefs, Hooks>::Variable var) {
	return p_varConfig.isPresent(var);
}

template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::varIsLocked(Config<BaseDefs, Hooks>::Variable var) {
	return false; //FIXME
}

template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::litTrue(Config<BaseDefs, Hooks>::Literal lit) {
	return p_varConfig.litIsTrue(lit);
}
template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::litFalse(Config<BaseDefs, Hooks>::Literal lit) {
	return p_varConfig.litIsFalse(lit);
}

template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::varIsFixed(Config<BaseDefs, Hooks>::Variable var) {
	return varDeclevel(var) == 1;
}

/* ------------------- CLAUSE MANAGEMENT FUNCTIONS ------------------------- */

template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::clauseContains(Config<BaseDefs, Hooks>::Clause clause,
		Config<BaseDefs, Hooks>::Literal literal) {
	for(auto i = clauseBegin(clause); i != clauseEnd(clause); ++i)
		if(*i == literal)
			return true;
	return false;
}

template<typename BaseDefs, typename Hooks>
int Config<BaseDefs, Hooks>::clausePolarity(Config<BaseDefs, Hooks>::Clause clause,
		Config<BaseDefs, Hooks>::Variable variable) {
	for(auto i = clauseBegin(clause); i != clauseEnd(clause); ++i) {
		Variable var = (*i).variable();
		if(var != variable)
			continue;
		return (*i).polarity();
	}
	return 0;
}

template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::clauseAssigned(Config<BaseDefs, Hooks>::Clause clause) {
	if(clauseLength(clause) == 1)
		return true;

	Literal lit1 = clauseGetFirst(clause);
	Literal lit2 = clauseGetSecond(clause);
	return varAssigned(lit1.variable()) && varAssigned(lit2.variable());
}

template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::clauseIsAntecedent(Config<BaseDefs, Hooks>::Clause clause) {
	Literal unit_lit = clauseGetFirst(clause);
	Variable unit_var = unit_lit.variable();
	auto antecedent = Antecedent::makeClause(clause);
	if(varAssigned(unit_var)
			&& antecedent == varAntecedent(unit_var))
		return true;
	return false;
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::ensureClauseSpace(unsigned int free_required) {
	if(free_required < p_clauseConfig.p_allocator.getFreeSpace())
		return;
	
	unsigned int used_estimate = p_clauseConfig.p_allocator.getUsedSpace();
	unsigned int final_space = 1.5f * used_estimate + free_required;

	if(opts.general.verbose >= 1)
		std::cout << "c [GC    ] Extending clause space to " << (final_space / 1024) << " kb" << std::endl;

	void *old_pointer = p_clauseConfig.p_allocator.getPointer();
	void *new_pointer = operator new(final_space);
	if(new_pointer == NULL)
		SYS_CRITICAL("Out of memory\n");
	p_clauseConfig.p_allocator.moveTo(new_pointer, final_space);
	operator delete(old_pointer);

	stat.general.clauseReallocs++;
}

template<typename BaseDefs, typename Hooks>
typename Config<BaseDefs, Hooks>::ClauseLitIndex Config<BaseDefs, Hooks>::clauseLength(
		Config<BaseDefs, Hooks>::Clause clause) {
	return p_clauseConfig.clauseLength(clause);
}

template<typename BaseDefs, typename Hooks>
typename Config<BaseDefs, Hooks>::ClauseLitIterator Config<BaseDefs, Hooks>::clauseBegin(
		Config<BaseDefs, Hooks>::Clause clause) {
	return ClauseLitIterator(p_clauseConfig, clause, 0);
}
template<typename BaseDefs, typename Hooks>
typename Config<BaseDefs, Hooks>::ClauseLitIterator Config<BaseDefs, Hooks>::clauseBegin2(
		Config<BaseDefs, Hooks>::Clause clause) {
	if(clauseLength(clause) < 2)
		return clauseEnd(clause);
	return ClauseLitIterator(p_clauseConfig, clause, 1);
}
template<typename BaseDefs, typename Hooks>
typename Config<BaseDefs, Hooks>::ClauseLitIterator Config<BaseDefs, Hooks>::clauseBegin3(
		Config<BaseDefs, Hooks>::Clause clause) {
	if(clauseLength(clause) < 3)
		return clauseEnd(clause);
	return ClauseLitIterator(p_clauseConfig, clause, 2);
}
template<typename BaseDefs, typename Hooks>
typename Config<BaseDefs, Hooks>::ClauseLitIterator Config<BaseDefs, Hooks>::clauseEnd(
		Config<BaseDefs, Hooks>::Clause clause) {
	return ClauseLitIterator(p_clauseConfig, clause,
			clauseLength(clause));
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::clauseSetEssential(Config<BaseDefs, Hooks>::Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, !p_clauseConfig.getFlagEssential(clause));
	currentEssentialClauses++;
	p_clauseConfig.setFlagEssential(clause);
}
template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::clauseUnsetEssential(Config<BaseDefs, Hooks>::Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, p_clauseConfig.getFlagEssential(clause));
	currentEssentialClauses--;
	p_clauseConfig.unsetFlagEssential(clause);
}
template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::clauseIsEssential(Config<BaseDefs, Hooks>::Clause clause) {
	return p_clauseConfig.getFlagEssential(clause);
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::markClause(Config<BaseDefs, Hooks>::Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, !p_clauseConfig.getFlagMarked(clause));
	p_clauseConfig.setFlagMarked(clause);
}
template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::unmarkClause(Config<BaseDefs, Hooks>::Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, p_clauseConfig.getFlagMarked(clause));
	p_clauseConfig.unsetFlagMarked(clause);
}
template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::clauseIsMarked(Config<BaseDefs, Hooks>::Clause clause) {
	return p_clauseConfig.getFlagMarked(clause);
}

// callback functor used during collectClauses()
template<typename Config>
class DelClauseCallback {
public:
	DelClauseCallback(Config &config) : p_config(config) { }

	void onErase(typename Config::Clause from_index) {
		SYS_ASSERT(SYS_ASRT_GENERAL, !p_config.p_clauseConfig.getFlagInstalled(from_index));
	}

	void onMove(typename Config::Clause from_index,
			typename Config::Clause to_index) {
		p_config.onClauseMove(from_index, to_index);
	}

private:
	Config &p_config;
};

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::collectClauses() {
	// allocate new space for the clauses
	unsigned int count_estimate = p_clauseConfig.numPresent();
	unsigned int used_estimate = p_clauseConfig.presentBytes();
	unsigned int space_estimate =  used_estimate + kClauseAlignment * count_estimate;
	unsigned int final_space = 1.5f * space_estimate + 4 * 1024 * 1024;
	
	ClauseConfig new_config;
	void *memory = operator new(final_space);
	new_config.p_allocator.initMemory(memory, final_space);
	
	// copy them to the new space
	DelClauseCallback<ThisType> callback(*this);
	relocateClauses(p_clauseConfig, new_config, callback);

	SYS_ASSERT(SYS_ASRT_GENERAL, new_config.p_allocator.getTotalSpace()
			- new_config.p_allocator.getFreeSpace() < space_estimate);

	// free the memory used by the old configuration
	operator delete(p_clauseConfig.p_allocator.getPointer());
	p_clauseConfig = std::move(new_config);

	// rebuild occurrence lists
	if(maintainOcclists) {
		for(auto it = varsBegin(); it != varsEnd(); ++it) {
			p_varConfig.occurClear((*it).oneLiteral());
			p_varConfig.occurClear((*it).zeroLiteral());
		}
		for(auto i = clausesBegin(); i != clausesEnd(); ++i)
			for(auto j = clauseBegin(*i); j != clauseEnd(*i); ++j)
				p_varConfig.occurInsert(*j, *i);
	}
	
	stat.general.clauseCollects++;
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::onClauseMove(Config<BaseDefs, Hooks>::Clause from_index,
		Config<BaseDefs, Hooks>::Clause to_index) {
	//std::cout << "Moving " << from_index << " to " << to_index << std::endl;
	/* replace the clause in both watch lists.
		NOTE: we cannot use clauseGetFirst on to_index! */
	if(!p_clauseConfig.getFlagInstalled(from_index))
		return;
	
	if(clauseLength(from_index) == 1) {
		// there is nothing to fix in this case
	}else if(clauseLength(from_index) == 2) {
		Literal lit1 = clauseGetFirst(from_index);
		Literal lit2 = clauseGetSecond(from_index);
		watchReplaceBinary(lit1.inverse(), from_index, to_index);
		watchReplaceBinary(lit2.inverse(), from_index, to_index);
	}else{
		Literal lit1 = clauseGetFirst(from_index);
		Literal lit2 = clauseGetSecond(from_index);
		watchReplaceClause(lit1.inverse(), from_index, to_index);
		watchReplaceClause(lit2.inverse(), from_index, to_index);
		
		// update the antecedent if the clause was unit
		Variable unit_var = lit1.variable();
		auto from_antecedent = Antecedent::makeClause(from_index);
		auto to_antecedent = Antecedent::makeClause(to_index);
		//if(varAssigned(unit_var))
		//	std::cout << "Antecedent: " << varAntecedent(unit_var).type
		//			<< " / " << varAntecedent(unit_var).identifier.padding << std::endl;
		if(from_antecedent == varAntecedent(unit_var))
			p_varConfig.setAntecedent(unit_var, to_antecedent);
	}
}

// checks whether a garbage collection is necessary
template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::checkClauseGarbage() {
	if(p_clauseConfig.numDeleted() > 0.5f * p_clauseConfig.numPresent())
		collectClauses();
	
	unsigned int count_estimate = p_clauseConfig.numClauses();
	unsigned int used_estimate = p_clauseConfig.presentBytes();
	unsigned int space_estimate =  used_estimate + kClauseAlignment * count_estimate;
	
	if(p_clauseConfig.p_allocator.getUsedSpace() > 2 * space_estimate)
		collectClauses();
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::expellContaining(Config<BaseDefs, Hooks>::Literal lit) {
	SYS_ASSERT(SYS_ASRT_GENERAL, currentAssignedVars == 0);
	
	// remove unit clauses
	auto unit_it = std::find(p_unitClauses.begin(), p_unitClauses.end(), lit);
	if(unit_it != p_unitClauses.end())
		p_unitClauses.erase(unit_it);
	
	// remove long clauses
	for(auto it = p_clauseConfig.begin(); it != p_clauseConfig.end(); ++it) {
		if(!clauseIsPresent(*it))
			continue;
		if(!clauseContains(*it, lit))
			continue;
		if(clauseIsEssential(*it))
			clauseUnsetEssential(*it);
		if(!clauseIsFrozen(*it))
			uninstallClause(*it);
		deleteClause(*it);
	}
}


/* -------------- WATCH LIST / OCCLIST FUNCTIONS ----------------------- */

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::watchInsertClause(Config<BaseDefs, Hooks>::Literal literal,
		Config<BaseDefs, Hooks>::Literal blocking,
		Config<BaseDefs, Hooks>::Clause clause) {
	WatchlistEntry new_entry;
	new_entry.setLong();
	new_entry.longSetClause(clause);
	new_entry.longSetBlocking(blocking);
	p_varConfig.watchInsert(literal, new_entry);
}
template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::watchInsertBinary(Config<BaseDefs, Hooks>::Literal literal,
		Config<BaseDefs, Hooks>::Literal implied,
		Config<BaseDefs, Hooks>::Clause clause) {
	WatchlistEntry new_entry;
	new_entry.setBinary();
	new_entry.binarySetImplied(implied);
	new_entry.binarySetClause(clause);
	p_varConfig.watchInsert(literal, new_entry);
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::watchRemoveClause(Config<BaseDefs, Hooks>::Literal literal,
		Config<BaseDefs, Hooks>::Clause clause) {
	for(auto i = p_varConfig.watchBegin(literal);
			i != p_varConfig.watchEnd(literal); ++i) {
		if(!(*i).isLong())
			continue;
		if((*i).longGetClause() != clause)
			continue;
		p_varConfig.watchErase(literal, i);
		return;
	}
	SYS_CRITICAL("Clause not found\n");
}
template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::watchRemoveBinary(Config<BaseDefs, Hooks>::Literal literal,
		Config<BaseDefs, Hooks>::Clause clause) {
	for(auto i = p_varConfig.watchBegin(literal);
			i != p_varConfig.watchEnd(literal); ++i) {
		if(!(*i).isBinary())
			continue;
		if((*i).binaryGetClause() != clause)
			continue;
		p_varConfig.watchErase(literal, i);
		return;
	}
	SYS_CRITICAL("Clause not found\n");
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::watchReplaceClause(Config<BaseDefs, Hooks>::Literal literal,
		Config<BaseDefs, Hooks>::Clause clause,
		Config<BaseDefs, Hooks>::Clause replacement) {
	for(auto i = watchBegin(literal); i != watchEnd(literal); ++i) {
		if(!(*i).isLong())
			continue;
		if((*i).longGetClause() != clause)
			continue;
		(*i).longSetClause(replacement);
		return;
	}
	SYS_CRITICAL("Clause not found\n");
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::watchReplaceBinary(Config<BaseDefs, Hooks>::Literal literal,
		Config<BaseDefs, Hooks>::Clause clause,
		Config<BaseDefs, Hooks>::Clause replacement) {
	for(auto i = watchBegin(literal); i != watchEnd(literal); ++i) {
		if(!(*i).isBinary())
			continue;
		if((*i).binaryGetClause() != clause)
			continue;
		(*i).binarySetClause(replacement);
		return;
	}
	SYS_CRITICAL("Clause not found\n");
}

template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::watchContainsBinary(Config<BaseDefs, Hooks>::Literal literal,
		Config<BaseDefs, Hooks>::Literal implied) {
	for(auto i = p_varConfig.watchBegin(literal);
			i != p_varConfig.watchEnd(literal); ++i) {
		if(!(*i).isBinary())
			continue;
		if((*i).binaryGetImplied() != implied)
			continue;
		return true;
	}
	return false;
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::occurConstruct() {
	SYS_ASSERT(SYS_ASRT_GENERAL, !maintainOcclists);
	for(auto i = clausesBegin(); i != clausesEnd(); ++i) {
		for(auto j = clauseBegin(*i); j != clauseEnd(*i); ++j)
			p_varConfig.occurInsert(*j, *i);
	}
	maintainOcclists = true;
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::occurDestruct() {
	SYS_ASSERT(SYS_ASRT_GENERAL, maintainOcclists);
	for(auto it = varsBegin(); it != varsEnd(); ++it) {
		p_varConfig.occurClear((*it).zeroLiteral());
		p_varConfig.occurShrink((*it).zeroLiteral());
		p_varConfig.occurClear((*it).oneLiteral());
		p_varConfig.occurShrink((*it).oneLiteral());
	}
	maintainOcclists = false;
}

/* ------------------- ASSIGN / UNASSIGN FUNCTIONS --------------------- */

template<typename Hooks>
class PropagateCallback {
public:
	void onUnit(typename Hooks::Literal literal) { }
};

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::propagate() {
	if(atConflict())
		return;
	while(p_propagateConfig.propagatePending()) {
		Literal literal = p_propagateConfig.nextPropagation();
		stat.search.propagations++;
		
		PropagateCallback<ThisType> callback;
		if(propagateWatch(*this, callback, literal))
			return;
	}
}

/* ------------------- ASSIGN / UNASSIGN FUNCTIONS --------------------- */

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::pushAssign(Config<BaseDefs, Hooks>::Literal literal,
		Config<BaseDefs, Hooks>::Antecedent antecedent) {
	// variables are never pushed twice!
	Variable var = literal.variable();
	SYS_ASSERT(SYS_ASRT_GENERAL, !varAssigned(var));
	
	p_propagateConfig.pushAssign(literal);

	// setup the antecedent and decision level of the variable
	bool is_one = literal == var.oneLiteral();
	p_varConfig.assign(var, is_one);
	p_varConfig.setDeclevel(var, curDeclevel());
	p_varConfig.setAntecedent(var, antecedent);
	currentAssignedVars++;

	// phase saving
	if(is_one) {
		p_varConfig.setVarFlagSaved(var);
	}else p_varConfig.clearVarFlagSaved(var);

	if(kReportAssign) {
		report<ReportTag>(ReportTag::kAssign);
		report<uint32_t>(literal.getIndex());
	}
	
//		std::cout << "[PUSH] Literal " << literal << " at order " << cur_order() 
//			<< " and decision " << curDeclevel() << std::endl;
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::popAssign() {
	Literal literal = p_propagateConfig.popAssign();

	Variable var = literal.variable();
	p_varConfig.unassign(var);
	currentAssignedVars--;

	// reinsert the variable into the heap
	if(!p_vsidsConfig.isInserted(var))
		p_vsidsConfig.insertVariable(var);

//		std::cout << "[POP] Literal " << literal << std::endl;
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::pushLevel() {
	p_propagateConfig.newDecision();
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::popLevel() {
	SYS_ASSERT(SYS_ASRT_GENERAL, curDeclevel() > 0);
	typename PropagateConfig::DecisionInfo decision = p_propagateConfig.peekDecision();
	p_propagateConfig.propagateReset();
	while(p_propagateConfig.currentOrder() > decision.firstIndex)
		popAssign();
	p_propagateConfig.popDecision();
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::backjump(Config<BaseDefs, Hooks>::Declevel to_declevel) {
//		std::cout << "Backjump to level " << to_declevel << std::endl;
//		std::cout << "   Current level was: " << curDeclevel() << std::endl;
	while(to_declevel < curDeclevel())
		popLevel();
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::restart() {
	backjump(2);
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::reset() {
	p_conflictDesc = Conflict::makeNone();
	backjump(0);
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::checkRestart() {
	if(opts.restart.strategy == kRestartLuby) {
		state.restart.lubyCounter++;
		if(state.restart.lubyCounter >= state.restart.lubyPeriod) {
			restart();
			stat.search.restarts++;
			state.restart.lubyPeriod = opts.restart.lubyScale
					* lubySequence(stat.search.restarts);
			state.restart.lubyCounter = 0;
		}
	}else if(opts.restart.strategy == kRestartGlucose) {

		// update the long average
		Declevel last_declevel = state.conflict.lastConflictDeclevel;
		state.restart.glucoseLongSum += last_declevel;

		// update the short average
		unsigned int glucose_ptr = state.restart.glucoseShortPointer;
		state.restart.glucoseShortSum -= state.restart.glucoseShortBuffer[glucose_ptr];
		state.restart.glucoseShortSum += last_declevel;
		state.restart.glucoseShortBuffer[glucose_ptr] = last_declevel;
		state.restart.glucoseShortPointer++;
		state.restart.glucoseShortPointer %= opts.restart.glucoseShortInterval;
	
		float long_avg = (float)state.restart.glucoseLongSum / conflictNum;
		float short_avg = (float)state.restart.glucoseShortSum
				/ opts.restart.glucoseShortInterval;

		state.restart.glucoseCounter++;
		if(state.restart.glucoseCounter > 100 && 0.7f * short_avg > long_avg) {
			restart();
			stat.search.restarts++;
			state.restart.glucoseCounter = 0;
			state.restart.glucoseShortSum = 0;
		}
	}else SYS_CRITICAL("Illegal restart strategy\n");
}

/* ----------------------------- INPUT FUNCTIONS --------------------------- */

template<typename BaseDefs, typename Hooks>
template<typename Iterator>
void Config<BaseDefs, Hooks>::inputClause(Config<BaseDefs, Hooks>::ClauseLitIndex length,
		Iterator begin, Iterator end) {
	Clause clause = allocClause(length, begin, end);
	clauseSetEssential(clause);
	installClause(clause);
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::inputFinish() {
	// initialize clause deletion heuristics
	unsigned int num_clauses = p_clauseConfig.numClauses();
	state.clauseRed.geomIncCounter = 0;
	state.clauseRed.geomIncLimit = 100;
	state.clauseRed.geomSizeLimit = num_clauses / 3 + 10;

	state.clauseRed.agileCounter = 0;
	state.clauseRed.agileInterval = opts.clauseRed.agileBaseInterval;
}

/* ------------------------ CLAUSE REDUCTION FUNCTIONS --------------------- */

template<typename Config>
class WorkClauseLt {
public:
	WorkClauseLt(Config &config) : p_config(config) { }

	bool operator() (typename Config::Clause left,
			typename Config::Clause right) {
		if((p_config.clauseGetLbd(left) < 4 || p_config.clauseGetLbd(right) < 4)
				&& (p_config.clauseGetLbd(left) != p_config.clauseGetLbd(right)))
			return p_config.clauseGetLbd(left) < p_config.clauseGetLbd(right);
		
		return p_config.clauseGetActivity(left)
				> p_config.clauseGetActivity(right);
	}

private:
	Config &p_config;
};

template<typename Config>
class ClauseGlueLt {
public:
	ClauseGlueLt(Config &config) : p_config(config) { }

	bool operator() (typename Config::Clause left,
			typename Config::Clause right) {
		if(p_config.clauseGetLbd(left) != p_config.clauseGetLbd(right))
			return p_config.clauseGetLbd(left) < p_config.clauseGetLbd(right);
		
		return p_config.clauseGetActivity(left)
				> p_config.clauseGetActivity(right);
	}

private:
	Config &p_config;
};

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::freezeClause(Config<BaseDefs, Hooks>::Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, !p_clauseConfig.getFlagFrozen(clause));
	uninstallClause(clause);
	p_clauseConfig.setFlagFrozen(clause);
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::quickFreezeClause(Config<BaseDefs, Hooks>::Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, !p_clauseConfig.getFlagInstalled(clause));
	SYS_ASSERT(SYS_ASRT_GENERAL, !p_clauseConfig.getFlagFrozen(clause));
	p_clauseConfig.setFlagFrozen(clause);
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::unfreezeClause(Config<BaseDefs, Hooks>::Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, p_clauseConfig.getFlagFrozen(clause));
	installClause(clause);
	p_clauseConfig.unsetFlagFrozen(clause);
}

template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::clauseIsFrozen(Config<BaseDefs, Hooks>::Clause clause) {
	return p_clauseConfig.getFlagFrozen(clause);
}

template<typename BaseDefs, typename Hooks>
int Config<BaseDefs, Hooks>::calculatePsm(Config<BaseDefs, Hooks>::Clause clause) {
	int psm = 0;
	for(auto it = clauseBegin(clause); it != clauseEnd(clause); ++it) {
		Variable var = (*it).variable();
		bool saved_one = p_varConfig.getVarFlagSaved(var);
		if(((*it).isOneLiteral() && saved_one)
				|| (!(*it).isOneLiteral() && !saved_one))
			psm++;
	}
	return psm;
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::reduceClauses() {
	SYS_ASSERT(SYS_ASRT_GENERAL, !atConflict());
	
	// build a list of all clauses that are considered for deletion
	std::vector<Clause> delete_queue;
	for(auto it = p_clauseConfig.begin(); it != p_clauseConfig.end(); ++it) {
		// never delete essential clauses
		if(clauseIsEssential(*it) || !clauseIsPresent(*it))
			continue;
		// do not delete classes that are currently unit
		if(clauseIsAntecedent(*it))
			continue;

		// TODO: does this make sense?
		// don't delete units and binary clauses
		if(clauseLength(*it) < 3)
			continue;
		// keep clauses with high activity
		if(clauseGetActivity(*it) > state.search.clauseActInc) {
			stat.clauseRed.clausesActive++;
		}else{
			stat.clauseRed.clausesNotActive++;
			delete_queue.push_back(*it);
		}
	}

	// TODO: just backjump to a lower level instead of resetting
	reset(); // reset the solver so that unfreeze works correctly
	
	for(unsigned int i = 0; i < delete_queue.size(); i++) {
		auto clause = delete_queue[i];
		
		if(clauseIsFrozen(clause)) {
			// unfreeze clauses if their psm is good
			if(calculatePsm(clause) <= 3) {
				unfreezeClause(clause);
				stat.clauseRed.clauseUnfreezes++;
			}
		}else{
			if(clauseGetLbd(clause) <= 8) {
				// freeze clauses that are good in general but bad under the current assignment
				freezeClause(clause);
				stat.clauseRed.clauseFreezes++;
			}else{
				uninstallClause(clause);
				deleteClause(clause);
				stat.clauseRed.clauseDeletions++;
			}
		}
		stat.clauseRed.clausesConsidered++;
	}
	stat.clauseRed.reductionRuns++;
	
	// restart the solver after the reset
	start();
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::checkClauseReduction() {
	if(opts.clauseRed.model == kClauseRedAgile) {
		state.clauseRed.agileCounter++;
		if(state.clauseRed.agileCounter
				>= state.clauseRed.agileInterval) {
			reduceClauses();
			state.clauseRed.numClauseReds++;

			state.clauseRed.agileCounter = 0;
			state.clauseRed.agileInterval += opts.clauseRed.agileSlowdown;
		}
	}else if(opts.clauseRed.model == kClauseRedGeometric) {
		if(currentActiveClauses - currentEssentialClauses
				>= state.clauseRed.geomSizeLimit)
			reduceClauses();
			state.clauseRed.numClauseReds++;

		state.clauseRed.geomIncCounter++;
		if(state.clauseRed.geomIncCounter
				>= state.clauseRed.geomIncLimit) {
			state.clauseRed.geomSizeLimit
					*= opts.clauseRed.geomSizeFactor;
			state.clauseRed.geomIncLimit
					*= opts.clauseRed.geomIncFactor;
			state.clauseRed.geomIncCounter = 0;
		}
	}else SYS_CRITICAL("Illegal clause reduction model\n");
}

/* ---------------------- DECISION FUNCTIONS ------------------------------- */

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::start() {
	SYS_ASSERT(SYS_ASRT_GENERAL, curDeclevel() == 0);
	
	// units are assigned on decision level 1
	pushLevel();
	if(!p_emptyClauses.empty()) {
		raiseConflict(Conflict::makeEmpty());
		return;
	}
	for(auto it = p_unitClauses.begin(); it != p_unitClauses.end(); ++it) {
		if(litTrue(*it))
			continue;
		if(litFalse(*it)) {
			raiseConflict(Conflict::makeFact(*it));
			return;
		}
			
		pushAssign(*it, Antecedent::makeDecision());
		propagate();
		if(atConflict())
			return;
	}
	
	// assumptions are assigned on decision level 2
	pushLevel();
	for(auto it = p_assumptionList.begin(); it != p_assumptionList.end(); ++it) {
		SYS_ASSERT(SYS_ASRT_GENERAL, p_varConfig.getLitFlagAssumption(*it));
		if(litTrue(*it))
			continue;
		if(litFalse(*it)) {
			raiseConflict(Conflict::makeFact(*it));
			return;
		}
		
		pushAssign(*it, Antecedent::makeDecision());
		propagate();
		if(atConflict())
			return;
	}
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::decide() {
	SYS_ASSERT(SYS_ASRT_GENERAL, curDeclevel() >= 2);
	SYS_ASSERT(SYS_ASRT_GENERAL, p_conflictDesc.isNone());
	
	// determine the variable with minimal VSIDS score
	Variable var = p_vsidsConfig.removeMaximum();
	while(varAssigned(var))
		var = p_vsidsConfig.removeMaximum();
		
	bool saved_one = p_varConfig.getVarFlagSaved(var);
	Literal decliteral = saved_one ? var.oneLiteral() : var.zeroLiteral();
	
	// create a new decision level; assign the variable
	pushLevel();
	pushAssign(decliteral, Antecedent::makeDecision());
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::scaleVarActivity(Config<BaseDefs, Hooks>::Activity divisor) {
	p_vsidsConfig.scaleActivity(divisor);
	state.search.varActInc /= divisor;
}
template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::scaleClauseActivity(Config<BaseDefs, Hooks>::Activity divisor) {
	for(auto i = clausesBegin(); i != clausesEnd(); ++i)
		clauseSetActivity(*i, clauseGetActivity(*i) / divisor);
	state.search.clauseActInc /= divisor;
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::onVarActivity(Config<BaseDefs, Hooks>::Variable var) {
	Activity activity = p_vsidsConfig.incActivity(var, state.search.varActInc);
	
	// re-scale the activity of all variables if necessary
	if(activity > 1.0E50)
		scaleVarActivity(activity);
}
template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::onAntecedentActivity(Config<BaseDefs, Hooks>::Antecedent antecedent) {
	if(antecedent.isClause()) {
		Clause clause = antecedent.getClause(); 
		double activity = clauseGetActivity(clause);
		clauseSetActivity(clause, activity + state.search.clauseActInc);
		
		// re-scale the activity of all clauses if necessary
		if(activity > 1.0E50)
			scaleClauseActivity(activity);
	}
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::increaseActivity() {
	state.search.varActInc *= state.search.varActFactor;
	state.search.clauseActInc *= state.search.clauseActFactor;
}

/* ------------------------- CONFLICT MANAGEMENT --------------------------- */

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::raiseConflict(Conflict conflict) {
//		std::cout << "Conflict! Variable: " << variable << std::endl;
	SYS_ASSERT(SYS_ASRT_GENERAL, !atConflict());
	p_conflictDesc = conflict;
	conflictNum++;
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::resetConflict() {
	p_conflictDesc = Conflict::makeNone();
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::resolveConflict() {
	SYS_ASSERT(SYS_ASRT_GENERAL, isResolveable());

	state.conflict.lastConflictDeclevel = curDeclevel();

	// determine the first uip clause
	p_learnConfig.cut(*this, conflictBegin(), conflictEnd());
	p_learnConfig.minimize(*this);
	p_learnConfig.build(*this);

	stat.search.learnedLits += p_learnConfig.cutSize();
	stat.search.minimizedLits += p_learnConfig.minSize();

	Literal uip_literal = p_learnConfig.getMin(0);
	Variable uip_var = uip_literal.variable();
	
	// generate a clause for the conflict
	SYS_ASSERT(SYS_ASRT_GENERAL, p_learnConfig.minSize() > 0);
	Clause learned = allocClause(p_learnConfig.minSize(),
			p_learnConfig.beginMin(), p_learnConfig.endMin());
	
	// calculate the literal-block-distance
	// NOTE: this has to be done before doing the backjump!
	unsigned int lbd = computeClauseLbd(*this, learned);
	clauseSetLbd(learned, lbd);
	clauseSetActivity(learned, state.search.clauseActInc);
	
	Declevel level = 0;
	if(p_learnConfig.minSize() > 1)
		level = varDeclevel(p_learnConfig.getMin(1).variable());
	
	// backjump and reset the conflict state
	if(level < 2) {
		backjump(0);
	}else{
		backjump(level);
	}
	p_conflictDesc = Conflict::makeNone();

	// assign the learned clause
	// NOTE: this has to be done after doing the backjump
	installClause(learned);
	if(level < 2) {
		start();
	}else{
		SYS_ASSERT(SYS_ASRT_GENERAL, p_learnConfig.minSize() >= 2);
		if(p_learnConfig.minSize() > 2) {
			pushAssign(uip_literal, Antecedent::makeClause(learned));

			// bump variables asserted by glue clauses as in glucose
			if(opts.learn.bumpGlueTwice)
				for(auto i = p_learnConfig.beginCurlevel();
						i != p_learnConfig.endCurlevel(); ++i) {
					Antecedent antecedent = varAntecedent(*i);
					/*if(antecedent.isBinary() && lbd > 2) {
						// cannot check if the clause is learned. do not bump for now!
						//onVarActivity(*i);
					}else if(antecedent.isClause()) {
						Clause reason = antecedent.identifier.clause_ident;
						if(!p_clauseConfig.getFlagEssential(reason)
								&& clauseGetLbd(reason) < lbd)
							onVarActivity(*i);
					}*/
				}
		}else if(p_learnConfig.minSize() == 2) {
			Literal reason_inverse = p_learnConfig.getMin(1);
			Literal reason_literal = reason_inverse.inverse();
			pushAssign(uip_literal, Antecedent::makeBinary(reason_literal));
		}
	}

	if(p_learnConfig.minSize() == 2) {
		stat.search.learnedBinary++;
	}else if(p_learnConfig.minSize() == 1) {
		stat.search.learnedUnits++;
	}
	
	p_hooks.onLearnedClause(learned);

	// output the drat proof line for this clause
	// TODO: move this to a "Hooks" function
	if(opts.general.outputDratProof) {
		for(auto it = p_learnConfig.beginMin(); it != p_learnConfig.endMin(); ++it) {
			std::cout << (*it).toNumber();
			std::cout << ' ';
		}
		std::cout << "0" << std::endl;
	}
	
	if(kReportSample) {
		report<ReportTag>(ReportTag::kSample);
		report<uint64_t>(currentActiveClauses);
	}

	// reset the learning configuration
	p_learnConfig.reset();
}

/* --------------------- ASSUMPTION FUNCTIONS -------------------------- */

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::assumptionEnable(Config<BaseDefs, Hooks>::Literal literal) {
	SYS_ASSERT(SYS_ASRT_GENERAL, !p_varConfig.getLitFlagAssumption(literal));
	SYS_ASSERT(SYS_ASRT_GENERAL, !varAssigned(literal.variable()));
	p_varConfig.setLitFlagAssumption(literal);

	p_assumptionList.push_back(literal);
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::assumptionDisable(Config<BaseDefs, Hooks>::Literal literal) {
	SYS_ASSERT(SYS_ASRT_GENERAL, p_varConfig.getLitFlagAssumption(literal));
	SYS_ASSERT(SYS_ASRT_GENERAL, !varAssigned(literal.variable()));
	p_varConfig.clearLitFlagAssumption(literal);

	auto it = std::find(p_assumptionList.begin(), p_assumptionList.end(), literal);
	SYS_ASSERT(SYS_ASRT_GENERAL, it != p_assumptionList.end());
	p_assumptionList.erase(it);
}

template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::isAssumed(Config<BaseDefs, Hooks>::Literal literal) {
	return p_varConfig.getLitFlagAssumption(literal);
}

/* --------------------- ALLOCATION FUNCTIONS -------------------------- */

template<typename BaseDefs, typename Hooks>
template<typename Iterator>
typename Config<BaseDefs, Hooks>::Clause Config<BaseDefs, Hooks>::allocClause(
		Config<BaseDefs, Hooks>::ClauseLitIndex length, Iterator begin, Iterator end) {
	unsigned int mem_estimate = p_clauseConfig.calcBytes(length)
			+ kClauseAlignment;
	
	// ensure that there is enough free space for a new clause
	if(p_clauseConfig.p_allocator.getFreeSpace() < mem_estimate)
		ensureClauseSpace(mem_estimate);
	
	// sanity check
	/*for(auto i = begin; i != end; ++i) {
		SYS_ASSERT(SYS_ASRT_GENERAL, p_varConfig.var_present((*i).variable()));
		for(auto j = begin; j != end; ++j) {
			if(i == j)
				continue;
			if(*i == *j || *i == (*j).inverse())
				SYS_CRITICAL("Illegal clause for clause_alloc()");
		}
	}*/
	
	Clause clause = p_clauseConfig.allocClause(length);

	ClauseSignature signature = 0;
	unsigned int sig_size = sizeof(ClauseSignature) * 8;
	ClauseLitIndex k = 0;
	for(Iterator it = begin; it != end && k < length; ++it, ++k) {
		p_clauseConfig.clauseSetLiteral(clause, k, *it);
		signature |= (1 << ((*it).getIndex() % sig_size));
	}
	clauseSetSignature(clause, signature);
	SYS_ASSERT(SYS_ASRT_GENERAL, k == length);
	
	if(maintainOcclists) {
		for(auto i = begin; i != end; ++i)
			p_varConfig.occurInsert(*i, clause);
	}
	
	return clause;
}

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::deleteClause(Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, !clauseIsEssential(clause));
	SYS_ASSERT(SYS_ASRT_GENERAL, !clauseIsInstalled(clause));
	SYS_ASSERT(SYS_ASRT_GENERAL, !p_clauseConfig.isDeleted(clause));
	p_clauseConfig.deleteClause(clause);
	
	if(maintainOcclists) {
		for(auto i = clauseBegin(clause); i != clauseEnd(clause); ++i)
			p_varConfig.occurRemove(*i, clause);
	}
	
	// output the drat proof line for this clause
	// TODO: move this to a "Hooks" function
	if(opts.general.outputDratProof) {
		std::cout << "d ";
		for(auto it = clauseBegin(clause); it != clauseEnd(clause); ++it) {
			std::cout << (*it).toNumber();
			std::cout << ' ';
		}
		std::cout << "0" << std::endl;
	}
	
	stat.general.deletedClauses++;
}

/* ---------------- INSTALL / UNINSTALL FUNCTIONS --------------------------- */

template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::installClause(Config<BaseDefs, Hooks>::Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, !p_clauseConfig.getFlagInstalled(clause));

	if(clauseLength(clause) == 0) {
		p_emptyClauses.push_back(clause);
	}else if(clauseLength(clause) == 1) {
		Literal lit = clauseGetFirst(clause);
		p_unitClauses.push_back(lit);
	}else if(clauseLength(clause) == 2) {
		Literal lit1 = clauseGetFirst(clause);
		Literal lit2 = clauseGetSecond(clause);
		watchInsertBinary(lit1.inverse(), lit2, clause);
		watchInsertBinary(lit2.inverse(), lit1, clause);
	}else{
		Literal lit1 = clauseGetFirst(clause);
		Literal lit2 = clauseGetSecond(clause);
		watchInsertClause(lit1.inverse(), lit2, clause);
		watchInsertClause(lit2.inverse(), lit1, clause);
	}
	
	p_clauseConfig.setFlagInstalled(clause);
	currentActiveClauses++;
}
template<typename BaseDefs, typename Hooks>
void Config<BaseDefs, Hooks>::uninstallClause(Config<BaseDefs, Hooks>::Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, !clauseIsAntecedent(clause));
	SYS_ASSERT(SYS_ASRT_GENERAL, p_clauseConfig.getFlagInstalled(clause));

	if(clauseLength(clause) == 1) {
		Literal lit = clauseGetFirst(clause);
		auto it = std::find(p_unitClauses.begin(), p_unitClauses.end(), lit);
		SYS_ASSERT(SYS_ASRT_GENERAL, it != p_unitClauses.end());
		p_unitClauses.erase(it);
	}else if(clauseLength(clause) == 2) {
		Literal lit1 = clauseGetFirst(clause);
		Literal lit2 = clauseGetSecond(clause);
		watchRemoveBinary(lit1.inverse(), clause);
		watchRemoveBinary(lit2.inverse(), clause);
	}else{
		Literal lit1 = clauseGetFirst(clause);
		Literal lit2 = clauseGetSecond(clause);
		watchRemoveClause(lit1.inverse(), clause);
		watchRemoveClause(lit2.inverse(), clause);
	}

	p_clauseConfig.unsetFlagInstalled(clause);
	currentActiveClauses--;
}
template<typename BaseDefs, typename Hooks>
bool Config<BaseDefs, Hooks>::clauseIsInstalled(Config<BaseDefs, Hooks>::Clause clause) {
	return p_clauseConfig.getFlagInstalled(clause);
}


struct TotalSearchStats {
	sys::HptCounter elapsed;

	TotalSearchStats() : elapsed(0) { }
};

template<typename Config>
void progressHeader(Config &config) {
	std::stringstream text;
	text << std::right;
	text << "c (" << std::to_string(config.p_configId) <<") [SEARCH]" ;
	text << std::setw(6) << "secs";
	text << std::setw(14) << "conflicts";
	text << std::setw(9) << "restarts";
	text << std::setw(12) << "limit";
	text << std::setw(10) << "space(kb)";
	std::cout << text.str() << std::endl;
	std::cout << "c -----------------------------------------------------------------------" << std::endl;
}

template<typename Config>
void progressMessage(Config &config, sys::HptCounter current_time) {
	std::stringstream msg;
	msg << "c (" << std::to_string(config.p_configId) << ") [SEARCH]";
	msg << std::right << std::setprecision(2) << std::fixed;
	msg << std::setw(6) << ((current_time - config.state.general.startTime) / (1000 * 1000 * 1000));
	msg << std::setw(14) << config.conflictNum;
	msg << std::setw(9) << config.stat.search.restarts;
	msg << std::setw(12) << (config.opts.clauseRed.model == Config::kClauseRedAgile
			? config.state.clauseRed.agileInterval
			: config.state.clauseRed.geomSizeLimit);
	msg << std::setw(10) << (config.usedClauseSpace() / 1024);
//			msg << std::setw(12) << ((float)config.state.restart.glucose_long_sum / config.conflictNum);
//			msg << std::setw(12) << ((float)config.state.restart.glucose_short_sum
//					/ config.opts.restart.glucose_short_interval);
	std::cout << msg.str() << std::endl;
}


template<typename Hooks>
void search(Hooks &hooks, satuzk::SolveState &cur_state,
		TotalSearchStats &total_stats) {
	auto start = sys::hptCurrent();

	for(unsigned int i = 0; true; i++) {
		while(hooks.atConflict()) {
			if(!hooks.isResolveable()) {
				total_stats.elapsed += sys::hptElapsed(start);
				if(hooks.isUnsatisfiable()) {
					cur_state = satuzk::SolveState::kStateUnsatisfiable;
				}else if(hooks.isFailedAssumption()) {
					cur_state = satuzk::SolveState::kStateAssumptionFail;
				}else SYS_CRITICAL("Illegal conflict state");
				return;
			}
			hooks.resolveConflict();
			hooks.increaseActivity();
			hooks.propagate();
		}
		
		hooks.checkRestart();
		hooks.checkClauseReduction();
		hooks.checkClauseGarbage();
		
		if(hooks.atLeaf()) {
			total_stats.elapsed += sys::hptElapsed(start);
			cur_state = satuzk::SolveState::kStateSatisfied;
			return;
		}else if(i > 1000) {
			// return after a certain conflict limit is reached
			total_stats.elapsed += sys::hptElapsed(start);
			cur_state = satuzk::SolveState::kStateBreak;
			return;
		}
		
		hooks.decide();
		hooks.propagate();
	}
}


}; /* namespace satuzk */

