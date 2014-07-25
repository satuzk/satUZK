
namespace satuzk {

enum SolveState {
	kStateUnknown,
	kStateBreak,
	kStateSatisfied,
	kStateUnsatisfiable,
	kStateAssumptionFail
};

// luby sequence:
// t(i) = 2^(k-1)				if i = 2^k - 1
// t(i) = t(i - 2^(k-1) + 1)	if 2^(k-1) <= i < 2^k - 1
inline unsigned int lubySequence(unsigned int i) {
	while(i != 0) {
		// calculate k maximal so that i <= 2^k - 1
		unsigned int rem = i;
		unsigned int k = 0;
		unsigned int kpow = 1; // k-th power of 2
		while(rem != 0) {
			k++;
			rem /= 2;
			kpow *= 2;
		}
		
		// first case: t(i) = 2^(k-1)
		if(i == kpow - 1)
			return kpow / 2;
		// second case: t(i) = t(i - 2^(k-1) + 1)
		i = i - kpow / 2 + 1;
	}
	return 1;
}

template<typename BaseDefs, typename Hooks>
class Config {
public:
	typedef LiteralType<BaseDefs> Literal;
	typedef VariableType<BaseDefs> Variable;
	typedef ClauseType<BaseDefs> Clause;
	typedef typename BaseDefs::ClauseLitIndex ClauseLitIndex;
	typedef typename BaseDefs::Order Order;
	typedef typename BaseDefs::Activity Activity;
	typedef typename BaseDefs::Declevel Declevel;

	static const int kClauseAlignment = 8;

	typedef Config<BaseDefs, Hooks> ThisType;

	typedef AntecedentStruct<BaseDefs> Antecedent;
	typedef ConflictStruct<BaseDefs> Conflict;

public:
	typedef VarConfigStruct<BaseDefs> VarConfig;
	typedef ClauseSpaceStruct<BaseDefs> ClauseConfig;

	typedef uint64_t ClauseSignature;

	typedef PropagateConfigStruct<BaseDefs> PropagateConfig;
	typedef LearnConfigStruct<BaseDefs> LearnConfig;
	typedef VsidsConfigStruct<BaseDefs> VsidsConfig;
	typedef ExtModelConfigStruct ExtModelConfig;

public:
	typedef typename VarConfig::OccurIterator OccurIterator;

	typedef typename VarConfig::WatchIterator WatchIterator;
	typedef typename VarConfig::WatchlistEntry WatchlistEntry;
	
	typedef typename ClauseConfig::ClauseIterator ClauseIterator;
	typedef ClauseLitIteratorStruct<BaseDefs> ClauseLitIterator;

	typedef AntecedentIteratorStruct<ThisType> AntecedentIterator;
	typedef ConflictIteratorStruct<ThisType> ConflictIterator;
	
public:
	Config(Hooks hooks, int config_id) : p_hooks(hooks), p_configId(config_id),
			p_conflictDesc(Conflict::makeNone()),
			maintainOcclists(false),
			conflictNum(0),
			currentAssignedVars(0),
			currentActiveClauses(0), currentEssentialClauses(0) {
		/* allocate memory for clauses */
		uint32_t clause_memsize = 4 * 1024 * 1024;
//		std::cout << "c [GC    ] Initial clause space: " << (clause_memsize / 1024) << " kb" << std::endl;
		void *memory = operator new(clause_memsize);
		p_clauseConfig.p_allocator.initMemory(memory, clause_memsize);

		p_rndEngine.seed(0x12345678);
		
		state.general.startTime = sys::hptCurrent();
		state.general.stopSolve = false; //FIXME: remove this flag
		
		state.restart.lubyCounter = 100;
		state.restart.lubyPeriod = opts.restart.lubyScale
			* lubySequence(stat.search.restarts);
		
		state.restart.glucoseShortBuffer = new Declevel[opts.restart.glucoseShortInterval];
		for(unsigned int i = 0; i < opts.restart.glucoseShortInterval; ++i)
			state.restart.glucoseShortBuffer[i] = 0;
		state.restart.glucoseShortPointer = 0;
		state.restart.glucoseShortSum = 0;
		state.restart.glucoseLongSum = 0;
		state.restart.glucoseCounter = 0;

		if(kReportEnable)
			p_reporter.open(p_configId);
	}

	~Config() {
		delete[] state.restart.glucoseShortBuffer;
		operator delete(p_clauseConfig.p_allocator.getPointer());
	}

	Config(const Config &) = delete;
	Config &operator= (const Config &) = delete;

	int getConfigId() { return p_configId; }

	void seedRandomEngine(unsigned long seed) {
		std::cout << "c random seed: 0x" << std::hex << seed << std::dec << std::endl;
		p_rndEngine.seed(seed);
	}
	
	/* ---------------- VARIABLE MANAGEMENT FUNCTIONS ---------------------- */
	// reserves space for variables; does not allocate them!
	void varReserve(typename BaseDefs::LiteralIndex count);

	// allocates a single variable
	Variable varAlloc();
	void deleteVar(Variable var);

	unsigned int numVariables() {
		return p_varConfig.count();
	}
	VarIterator<BaseDefs> varsBegin() {
		return VarIterator<BaseDefs>(0);
	}
	VarIterator<BaseDefs> varsEnd() {
		return VarIterator<BaseDefs>(p_varConfig.count());
	}
	
	// initializes the VSIDS heuristic
	void randomizeVsids();

	bool varIsPresent(Variable var);

	// locked variables must not be removed from clauses
	// and must not be eliminated
	void lockVariable(Variable var);
	void unlockVariable(Variable var);
	bool varIsLocked(Variable var);
	
	Bool3 varState(Variable var) {
		if(!varAssigned(var))
			return undefined3();
		return (varOne(var) ? true3() : false3());
	}
	bool varAssigned(Variable var) {
		return p_varConfig.isAssigned(var);
	}
	bool varOne(Variable var) {
		return p_varConfig.isAssignedOne(var);
	}
	bool varZero(Variable var) {
		return p_varConfig.isAssignedZero(var);
	}
	
	// returns true if a literal is currently assigned true
	bool litTrue(Literal lit);
	// returns true if a literal is currently assigned false
	bool litFalse(Literal lit);
	Bool3 litState(Literal lit) {
		return varState(lit.variable()) ^ !lit.isOneLiteral();
	}

	Antecedent varAntecedent(Variable var) {
		return p_varConfig.getAntecedent(var);
	}
	Declevel varDeclevel(Variable var) {
		return p_varConfig.getDeclevel(var);
	}
	// returns true if this variable is fixed by a unit clause
	//FIXME: why do we need this?
	bool varIsFixed(Variable var);

	Literal litEquivalent(Literal literal) {
		return p_varConfig.equivalent(literal);
	}
	void litJoin(Literal lit1, Literal lit2) {
		p_varConfig.join(lit1, lit2);
	}

	Activity varGetActivity(Variable var) {
		return p_vsidsConfig.getActivity(var);
	}

	// returns true if the model value of the variable is one
	bool modelGetVariable(Variable var) {
		return p_varConfig.getVarFlagModel(var);
	}
	void modelSetVariable(Variable var, bool value) {
		//TODO: allow unassigned variables in model
		p_varConfig.clearVarFlagModel(var);
		if(value)
			p_varConfig.setVarFlagModel(var);
	}
	// returns true if the model value of the literal is true
	bool modelGetLiteral(Literal lit) {
		if(lit.isOneLiteral())
			return modelGetVariable(lit.variable());
		return !modelGetVariable(lit.variable());
	}
	
	/* ----------------- CLAUSE MANAGEMENT FUNCTIONS ----------------------- */

	bool clauseContains(Clause clause, Literal literal);
	int clausePolarity(Clause clause, Variable variable);

	/* make sure there is enough space to store clauses.
	 * move the whole clause space if there is not. */
	void ensureClauseSpace(unsigned int free_required);
	
	unsigned int usedClauseSpace() {
		return p_clauseConfig.p_allocator.getUsedSpace();
	}

	ClauseIterator clausesBegin() {
		return p_clauseConfig.begin();
	}
	ClauseIterator clausesEnd() {
		return p_clauseConfig.end();
	}

	bool clauseIsPresent(Clause clause) {
		return !p_clauseConfig.isDeleted(clause);
	}

	ClauseLitIndex clauseLength(Clause clause);
	
	std::string printClause(Clause clause) {
		std::string string = "{";
		for(auto i = clauseBegin(clause); i != clauseEnd(clause); ++i) {
			if(i != clauseBegin(clause))
				string += ", ";
			string += std::to_string((*i).toNumber());
		}
		string += "}";
		return string;
	}

	ClauseLitIterator clauseBegin(Clause clause);
	ClauseLitIterator clauseBegin2(Clause clause);
	ClauseLitIterator clauseBegin3(Clause clause);
	ClauseLitIterator clauseEnd(Clause clause);

	void clauseSetFirst(Clause clause, Literal literal) {
		p_clauseConfig.clauseSetLiteral(clause, 0, literal);
	}
	void clauseSetSecond(Clause clause, Literal literal) {
		p_clauseConfig.clauseSetLiteral(clause, 1, literal);
	}

	Literal clauseGetLiteral(Clause clause, ClauseLitIndex index) {
		return p_clauseConfig.clauseGetLiteral(clause, index);
	}
	Literal clauseGetFirst(Clause clause) {
		return p_clauseConfig.clauseGetLiteral(clause, 0);
	}
	Literal clauseGetSecond(Clause clause) {
		return p_clauseConfig.clauseGetLiteral(clause, 1);
	}

	void clauseSetEssential(Clause clause);
	void clauseUnsetEssential(Clause clause);
	bool clauseIsEssential(Clause clause);

	void markClause(Clause clause);
	void unmarkClause(Clause clause);
	bool clauseIsMarked(Clause clause);
	
	/* returns true if the clause is currently unit or unsatisfied.
	 * only returns a valid result after propagation is done */
	// FIXME: do we need this?
	bool clauseAssigned(Clause clause);
	// returns true if the clause is currently the antecedent of a variable
	bool clauseIsAntecedent(Clause clause);
	
	unsigned int clauseGetLbd(Clause clause) {
		return p_clauseConfig.clauseGetLbd(clause);
	}
	void clauseSetLbd(Clause clause, unsigned int lbd) {
		if(lbd > UINT16_MAX)
			lbd = UINT16_MAX;
		p_clauseConfig.clauseSetLbd(clause, lbd);
	}
	
	Activity clauseGetActivity(Clause clause) {
		return p_clauseConfig.clauseGetActivity(clause);
	}
	void clauseSetActivity(Clause clause, Activity activity) {
		p_clauseConfig.clauseSetActivity(clause, activity);
	}

	ClauseSignature clauseGetSignature(Clause clause) {
		return p_clauseConfig.clauseGetSignature(clause);
	}
	void clauseSetSignature(Clause clause, ClauseSignature signature) {
		p_clauseConfig.clauseSetSignature(clause, signature);
	}

	// use a copying-garbage collection to get rid of deleted clauses
	void collectClauses();

	/* helper function for collectClauses().
	 * Called when a clause is moved. Updates watch lists etc. */
	void onClauseMove(Clause from_index, Clause to_index);
	
	// checks whether a garbage collection is necessary
	void checkClauseGarbage();

	// removes all clauses containing the specified literal
	void expellContaining(Literal lit);

	/* -------------- WATCH LIST / OCCLIST FUNCTIONS ----------------------- */

	WatchIterator watchBegin(Literal literal) {
		return p_varConfig.watchBegin(literal);
	}
	WatchIterator watchEnd(Literal literal) {
		return p_varConfig.watchEnd(literal);
	}
	void watchCut(Literal literal, WatchIterator iterator) {
		return p_varConfig.watchCut(literal, iterator);
	}
	
	// helper functions to manipulate watch lists
	void watchInsertClause(Literal literal, Literal blocking, Clause clause);
	void watchInsertBinary(Literal literal, Literal implied, Clause clause);
	
	// helper functions to manipulate watch lists
	void watchRemoveClause(Literal literal, Clause clause);
	void watchRemoveBinary(Literal literal, Clause clause);
	
	void watchReplaceClause(Literal literal, Clause clause, Clause replacement);
	void watchReplaceBinary(Literal literal, Clause clause, Clause replacement);
	//TODO: do we need this?
	bool watchContainsBinary(Literal literal, Literal implied);

	void occurConstruct();
	void occurDestruct();

	unsigned int occurSize(Literal literal) {
		return p_varConfig.occurSize(literal);
	}
	OccurIterator occurBegin(Literal literal) {
		return p_varConfig.occurBegin(literal);
	}
	OccurIterator occurEnd(Literal literal) {
		return p_varConfig.occurEnd(literal);
	}

	/* ----------------------- PROPAGATION FUNCTIONS ----------------------- */

	void propagate();

	Declevel curDeclevel() {
		return p_propagateConfig.curDeclevel();
	}
	Order curOrder() {
		return p_propagateConfig.currentOrder();
	}
	Literal getOrder(Order order) {
		return p_propagateConfig.getAssignByOrder(order);
	}

	AntecedentIterator causesBegin(Antecedent antecedent) {
		return AntecedentIterator::begin(*this, antecedent);
	}
	AntecedentIterator causesEnd(Antecedent antecedent) {
		return AntecedentIterator::end(*this, antecedent);
	}
	ConflictIterator conflictBegin() {
		return ConflictIterator::begin(*this, p_conflictDesc);
	}
	ConflictIterator conflictEnd() {
		return ConflictIterator::end(*this, p_conflictDesc);
	}
	
	/* ------------------- ASSIGN / UNASSIGN FUNCTIONS --------------------- */

	void pushAssign(Literal literal, Antecedent antecedent);
	void popAssign();
	
	// creates a new decision level
	void pushLevel();
	// undos a single decision
	void popLevel();
	
	// undos all decisions up to (but not including!) the specified level
	void backjump(Declevel to_declevel);
	
	// restarts the search from scratch
	void restart();
	
	/* resets the solver state so that another search (e.g. with new clauses or
	 * different assumptions) can be started */
	void reset();

	void checkRestart();
	
	/* ------------------------ INPUT FUNCTIONS ---------------------------- */
	template<typename Iterator>
	void inputClause(ClauseLitIndex length, Iterator begin, Iterator end);
	
	void inputFinish();

	/* --------------- CLAUSE REDUCTIONS FUNCTIONS ------------------------- */
	
	// sets the frozen flag and uninstalls the clause
	void freezeClause(Clause clause);
	// sets the frozen flag for clauses that are currently not installed
	void quickFreezeClause(Clause clause);
	void unfreezeClause(Clause clause);
	bool clauseIsFrozen(Clause clause);

	int calculatePsm(Clause clause);

	// deletes clauses in order to decrease the number of learned clauses
	void reduceClauses();

	// checks if clause reduction should be done
	void checkClauseReduction();

	/* ------------------ DECISION FUNCTIONS ------------------------------- */

	// assigns all unit clauses and assumptions
	void start();

	// decides which variable should be assigned next and assigns the variable.
	void decide();
	
	// returns true if we are at a satisfiable assignment
	bool atLeaf() { return currentAssignedVars == p_varConfig.count(); }

	// helper functions to rescale activity in order to prevent overflows
	void scaleVarActivity(Activity divisor);
	void scaleClauseActivity(Activity divisor);
	
	// update activity heuristics
	void onVarActivity(Variable var);
	void onAntecedentActivity(Antecedent antecedent);

	void increaseActivity();

	/* ------------------ CONFLICT MANAGEMENT ------------------------------ */

	void raiseConflict(Conflict conflict);
	void resetConflict();

	bool atConflict() { return !p_conflictDesc.isNone(); }

	bool isResolveable() {
		SYS_ASSERT(SYS_ASRT_GENERAL, atConflict());
		return curDeclevel() > 2;
	}
	bool isUnsatisfiable() {
		SYS_ASSERT(SYS_ASRT_GENERAL, atConflict());
		return curDeclevel() == 1;
	}
	bool isFailedAssumption() {
		SYS_ASSERT(SYS_ASRT_GENERAL, atConflict());
		return curDeclevel() == 2;
	}
	
	// resolves the conflict
	void resolveConflict();

	/* --------------------- ASSUMPTION FUNCTIONS -------------------------- */
	void assumptionEnable(Literal literal);
	void assumptionDisable(Literal literal);
	bool isAssumed(Literal literal);
	
	/* --------------------- ALLOCATION FUNCTIONS -------------------------- */

	/* allocates a clause and fills it with the given literals.
		NOTE: does NOT install watched literal etc. */
	template<typename Iterator>
	Clause allocClause(ClauseLitIndex length, Iterator begin, Iterator end);

	void deleteClause(Clause clause);

	/* -------------- INSTALL / UNINSTALL FUNCTIONS ------------------------ */
	/* invalidates watch list iterators for the *inverses* of
	 *     the first two literals of the clause.
	 * invalidates occlist iterators for all literals of the clause. */
	void installClause(Clause clause);
	void uninstallClause(Clause clause);
	bool clauseIsInstalled(Clause clause);
	
	/* -------------------- RESOLVENT CALCULATION -------------------------- */
	// TODO: should not be part of Config
	
	// returns true if the resolvent is trivial
	bool resolventTrivial(Clause clause1, Clause clause2,
			Variable variable) {
		for(auto i = clauseBegin(clause1); i != clauseEnd(clause1); ++i) {
			auto var = (*i).variable();
			if(var == variable)
				continue;
			int polarity1 = (*i).polarity();
			int polarity2 = clausePolarity(clause2, var);
			if(polarity1 == -polarity2)
				return true;
		}
		return false;
	}
	
	// returns true if the resolvent is trivial. calculates its size otherwise
	bool resolventLength(Clause clause1, Clause clause2,
			Variable variable, unsigned int &length) {
		length = clauseLength(clause1) + clauseLength(clause2) - 2;
		for(auto i = clauseBegin(clause1); i != clauseEnd(clause1); ++i) {
			auto var = (*i).variable();
			if(var == variable)
				continue;
			int polarity1 = (*i).polarity();
			int polarity2 = clausePolarity(clause2, var);
			if(polarity1 == -polarity2)
				return true;
			if(polarity1 == polarity2)
				length--;
		}
		return false;
	}

	// builds the resolvent of the two given clauses
	std::vector<Literal> resolventBuild(Clause clause1,
			Clause clause2, Variable variable) {
		std::vector<Literal> result;
		for(auto i = clauseBegin(clause1); i != clauseEnd(clause1); ++i) {
			if((*i).variable() == variable)
				continue;
			result.push_back(*i);
		}
		for(auto i = clauseBegin(clause2); i != clauseEnd(clause2); ++i) {
			if((*i).variable() == variable)
				continue;
			if(clauseContains(clause1, *i))
				continue;
			result.push_back(*i);
		}
		return result;
	}
	
	/* -------- HELPER FUNCTIONS: LBD CALCULATION -------------------------- */
	std::vector<bool> lbd_buffer;
	int lbd_counter;
	
	void lbdInit() {
		if(lbd_buffer.size() < curDeclevel() + 1)
			lbd_buffer.resize(curDeclevel() + 1, false);
		lbd_counter = 0;
	}
	void lbdInsert(Declevel declevel) {
		SYS_ASSERT(SYS_ASRT_GENERAL, declevel < lbd_buffer.size());
		if(lbd_buffer[declevel])
			return;
		lbd_buffer[declevel] = true;
		lbd_counter++;
	}
	void lbdCleanup(Declevel declevel) {
		lbd_buffer[declevel] = false;
	}
	int lbdResult() {
		return lbd_counter;
	}

public:
	Hooks p_hooks;

	int p_configId;	

	VarConfig p_varConfig;
	ClauseConfig p_clauseConfig;
	
	// stores all unit clauses
	std::vector<Literal> p_unitClauses;
	// stores all active assumptions
	std::vector<Literal> p_assumptionList;
	
	Conflict p_conflictDesc;

	PropagateConfig p_propagateConfig;
	LearnConfig p_learnConfig;
	VsidsConfig p_vsidsConfig;
	ExtModelConfig p_extModelConfig;

	std::mt19937 p_rndEngine;
	
	bool maintainOcclists;

	// number of conflicts resolved so far
	uint64_t conflictNum;
	// number of variables currently assigned. used to determine if the instance is sat
	typename BaseDefs::LiteralIndex currentAssignedVars;
	// number of currently active clauses. used for clause reduction heuristics
	uint32_t currentActiveClauses;
	uint32_t currentEssentialClauses;

	sys::Reporter p_reporter;

	const bool kReportEnable = false;
	const bool kReportAssign = false, kReportSample = false, kReportReducerSample = false;

	enum class ReportTag : uint16_t {
		kNone, kAssign, kConflict, kSample, kReducerSample
	};

	template<typename T>
	void report(const T &data) {
		if(kReportEnable)
			p_reporter.write(data);
	}

	enum ClauseRedModel {
		kClauseRedAgile,
		kClauseRedGeometric
	};
	
	enum RestartStrategy {
		kRestartLuby,
		kRestartGlucose
	};

	struct {
		struct StatGeneral {
			uint32_t clauseReallocs;
			uint32_t clauseCollects;
			
			uint64_t deletedClauses;
			
			StatGeneral() :
				clauseReallocs(0),
				clauseCollects(0),
				deletedClauses(0) { }
		} general;

		struct StatSearch {
			uint32_t factElimRuns;
			uint64_t factElimClauses;
			uint64_t factElimLiterals;
			
			uint64_t propagations;
			uint64_t learnedLits, learnedUnits, learnedBinary;
			uint64_t minimizedLits;
			uint32_t restarts;
			sys::HptCounter prop_time;
			
			StatSearch() :
				factElimRuns(0),
				factElimClauses(0),
				factElimLiterals(0),
					
				propagations(0),
				learnedLits(0), learnedUnits(0), learnedBinary(0),
				minimizedLits(0),
				restarts(0),
				prop_time(0) { }
		} search;

		struct StatClauseRed {
			uint64_t clausesActive, clausesNotActive, clausesConsidered;
			uint64_t clauseDeletions, clauseFreezes, clauseUnfreezes;
			uint32_t reductionRuns;
			
			StatClauseRed() : clausesActive(0), clausesNotActive(0), clausesConsidered(0),
				clauseDeletions(0), clauseFreezes(0), clauseUnfreezes(0),
				reductionRuns(0) { }
		} clauseRed;

		struct StatSimp {
			uint64_t vecdEliminated, vecdFacts;
			uint64_t bceEliminated;
			uint64_t sccVariables;
			uint64_t selfsubShortcuts;
			uint64_t selfsubSubChecks;
			uint64_t selfsubResChecks;
			uint64_t selfsubFacts;
			uint64_t selfsubClauses;
			uint64_t subsumed_clauses;
			uint64_t distChecks;
			uint64_t distConflicts;
			uint64_t distConflictsRemoved;
			uint64_t distAsserts;
			uint64_t distAssertsRemoved;
			uint64_t distSelfSubs;
			uint64_t distSelfSubsRemoved;
			uint64_t unhideTransitiveEdges;
			uint64_t unhideFailedLiterals;
			uint64_t unhideHleLiterals;
			uint64_t unhideHteClauses;

			StatSimp() :
				vecdEliminated(0), vecdFacts(0),
				bceEliminated(0),
				sccVariables(0),
				selfsubShortcuts(0), selfsubSubChecks(0), selfsubResChecks(0),
					selfsubFacts(0), selfsubClauses(0),
				subsumed_clauses(0),
				distChecks(0),
				distConflicts(0), distConflictsRemoved(0), distAsserts(0), distAssertsRemoved(0),
					distSelfSubs(0), distSelfSubsRemoved(0),
				unhideTransitiveEdges(0), unhideFailedLiterals(0),
					unhideHleLiterals(0), unhideHteClauses(0) { }
		} simp;
	} stat;

	struct StatPerf {
		sys::HptCounter preprocTime;
		sys::HptCounter inprocTime;
		
		StatPerf() : preprocTime(0), inprocTime(0) { }
	} perf;

	struct {
		struct OptsGeneral {
			int verbose;

			sys::HptCounter budget;
			sys::HptCounter timeout;
			
			bool outputDratProof;

			OptsGeneral() : verbose(0),
					budget(0), timeout(0),
					outputDratProof(false) { }
		} general;

		struct OptsLearn {
			bool bumpGlueTwice;
			bool minimizeGlucose;
			
			OptsLearn() :
				bumpGlueTwice(false),
				minimizeGlucose(false) { }
		} learn;

		struct OptsClauseRed {
			ClauseRedModel model;

			// initial learned clauses limit
			uint32_t agileBaseInterval;
			// linear slowdown of the clause limit
			uint32_t agileSlowdown;

			// growth factor for geomSizeLimit
			double geomSizeFactor;
			// growth factor for geomIncLimit
			double geomIncFactor;
			
			OptsClauseRed() : model(kClauseRedAgile),
					agileBaseInterval(500), agileSlowdown(100),
					geomSizeFactor(1.1f), geomIncFactor(1.1f) { }
		} clauseRed;

		struct OptsRestart {
			RestartStrategy strategy;
			uint32_t lubyScale;
			static const unsigned int glucoseShortInterval = 100;

			OptsRestart() : strategy(kRestartLuby), lubyScale(100) { }
		} restart;
	} opts;

	struct {
		struct {
			sys::HptCounter startTime;
			volatile bool stopSolve;
		} general;

		struct StateSearch {
			// increment that is added to the score on each activity
			Activity varActInc;
			// the increment is multiplies by this factor each conflict 
			Activity varActFactor;

			// see the corresponding varAct... variables
			Activity clauseActInc;
			Activity clauseActFactor;
			
			StateSearch() : varActInc(1.0f), varActFactor(1.05f),
					clauseActInc(1.0f), clauseActFactor(1.05f) { }
		} search;

		struct StateClauseRed {
			// number of clause reductions so far
			uint32_t numClauseReds;
			
			// number of conflicts since the last reduce
			uint64_t agileCounter;
			// number of conflicts between two reduce runs
			uint64_t agileInterval;
			
			// current learned clauses limit
			uint64_t geomSizeLimit;
			// number of conflicts since last increment
			uint64_t geomIncCounter;
			// number of conflicts before next increment
			uint64_t geomIncLimit;

			StateClauseRed() : numClauseReds(0) { }
		} clauseRed;

		struct {
			uint32_t lubyCounter;
			uint32_t lubyPeriod;
			
			Declevel *glucoseShortBuffer;
			unsigned int glucoseShortPointer;
			unsigned int glucoseCounter;
			uint64_t glucoseShortSum;
			uint64_t glucoseLongSum;
		} restart;

		struct {
			Declevel lastConflictDeclevel;
		} conflict;
	} state;
};

};

