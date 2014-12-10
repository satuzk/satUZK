
#include <unistd.h>
#include <algorithm>
#include <array>
#include <vector>
#include <map>
#include <queue>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <stdexcept>
#include <cstring>
#include <csignal>
#include <cassert>

#include "../Config.hpp"

#include "../inline/sys/Debug.hpp"
#include "../inline/sys/Reporter.hpp"
#include "../inline/sys/Performance.hpp"
#include "../inline/util/BulkAlloc.hpp"
#include "../inline/util/BinaryHeap.hpp"
#include "../inline/util/BinaryStack.hpp"
#include "../inline/util/Bool3.hpp"
#include "../inline/util/SimpleVector.hpp"

#include "../include/Antecedent.hpp"
#include "../include/Conflict.hpp"
#include "../include/Vars.hpp"

#include "../inline/Antecedent.hpp"
#include "../inline/Conflict.hpp"
#include "../inline/Vars.hpp"
#include "../inline/Clauses.hpp"

#include "../inline/Propagate.hpp"
#include "../inline/Lbd.hpp"
#include "../inline/Learn.hpp"
#include "../inline/Vsids.hpp"
#include "../inline/ExtModel.hpp"
#include "../include/Config.hpp"
#include "../inline/simplify/VarElim.hpp"
#include "../inline/simplify/BlockedClauseElim.hpp"
#include "../inline/simplify/Subsumption.hpp"
#include "../inline/simplify/Unhiding.hpp"
#include "../inline/simplify/Equivalent.hpp"
#include "../inline/Dimacs.hpp"
#include "../inline/Config.hpp"

#ifdef FEATURE_GOOGLE_PROFILE
#include <google/profiler.h>
#endif

struct BaseDefs {
	typedef uint32_t LiteralIndex;
	typedef uint32_t ClauseIndex;
	typedef uint32_t ClauseLitIndex;
	typedef uint32_t Order;
	typedef double Activity;
	typedef uint32_t Declevel;
};

struct OurHooks {
	void onLearnedClause(satuzk::ClauseType<BaseDefs> clause) { }
};

typedef satuzk::Config<BaseDefs, OurHooks> OurConfig;

OurConfig *the_config = new OurConfig(OurHooks(), 1);

class CnfReadHooks {
public:
	CnfReadHooks(OurConfig &config) : p_config(config) { }
	
	void onProblem(long num_vars, long num_clauses) {
		p_config.varReserve(num_vars);
		for(long i = 0; i < num_vars; ++i)	
			p_config.varAlloc();
		p_varCount = num_vars;
		std::cout << "c var memory: " << sysPeakMemory() << " kb" << std::endl;
	}

	void onClause(std::vector<long> &in_clause) {
		// transform input variable ids to internal variable ids
		std::vector<typename OurConfig::Literal> out_clause;
		for(auto it = in_clause.begin(); it != in_clause.end(); ++it) {
			long input_variable = (*it) < 0 ? -(*it) : (*it);
			SYS_ASSERT(SYS_ASRT_GENERAL, input_variable <= p_varCount);
			
			typename OurConfig::Variable intern_variable = internVariable(input_variable);
			typename OurConfig::Literal intern_literal = (*it) < 0
					? intern_variable.zeroLiteral()
					: intern_variable.oneLiteral();
			out_clause.push_back(intern_literal);
		}
		
		// permutate the input clause
		//std::shuffle(out_clause.begin(), out_clause.end(),
		//		p_config.p_rndEngine);
		
		p_config.inputClause(out_clause.size(),
				out_clause.begin(), out_clause.end());
	}

	int numVariables() { return p_varCount; }
	typename OurConfig::Variable internVariable(int input_variable) {
		return OurConfig::Variable::fromIndex(input_variable - 1);
	}

private:
	long p_varCount;
	OurConfig &p_config;
};

satuzk::SolveState solve(OurConfig &config) {
	if(config.opts.general.verbose >= 1) {
		std::cout << "c [      ]  initial:" << std::endl;
		std::cout << "c [      ]     variables: " << config.p_varConfig.presentCount()
			<< ", clauses: " << config.p_clauseConfig.numPresent()
			<< ", watch lists: " << config.p_varConfig.watchOverallSize() << std::endl;	
	}
	
	// perform preprocessing	
	if(config.opts.general.verbose >= 1)
		std::cout << "c building occurrence lists" << std::endl;
	config.occurConstruct();

	auto preproc_start = sys::hptCurrent();

	UnhideRunStats stat_unhide;
	bceEliminateAll(config);
	vecdEliminateAll(config);
	selfsubEliminateAll(config);
	for(int i = 0; i < 5; i++)
		unhideEliminateAll(config, true, stat_unhide);

	config.perf.preprocTime += sys::hptElapsed(preproc_start);
	
	config.occurDestruct();
	
	// start the search
	if(config.opts.general.verbose >= 1) {
		std::cout << "c [      ]  before search:" << std::endl;
		std::cout << "c [      ]     variables: " << config.p_varConfig.presentCount()
			<< ", clauses: " << config.p_clauseConfig.numPresent()
			<< ", watch lists: " << config.p_varConfig.watchOverallSize() << std::endl;	
	}
	
	auto last_message = sys::hptCurrent();
	if(config.opts.general.verbose >= 1)
		progressHeader(config);

	satuzk::TotalSearchStats search_total_stats;

	config.start();

	while(true) {
		if(config.state.general.stopSolve)
			return satuzk::SolveState::kStateBreak;
		
		auto current = sys::hptCurrent();
		if(config.opts.general.timeout != 0 && config.opts.general.timeout
				< current - config.state.general.startTime)
			return satuzk::SolveState::kStateBreak;
		if(current - last_message > 5LL * 1000 * 1000 * 1000) {
			if(config.opts.general.verbose >= 1)
				progressMessage(config, current);
			last_message = current;
		}

		satuzk::SolveState cur_state = satuzk::SolveState::kStateUnknown;
		search(config, cur_state, search_total_stats);
		if(cur_state == satuzk::SolveState::kStateSatisfied
				|| cur_state == satuzk::SolveState::kStateUnsatisfiable
				|| cur_state == satuzk::SolveState::kStateAssumptionFail) 
			return cur_state;
	}
}

void onInterrupt(int sig) {
	std::cout << "c signal: ";
	switch(sig) {
		case SIGINT: std::cout << "SIGINT"; break;
		case SIGXCPU: std::cout << "SIGXCPU"; break;
		default: std::cout << sig;
	}
	std::cout << std::endl;
	the_config->state.general.stopSolve = true;
}

int main(int argc, char **argv) {
	std::cout << "c this is satUZK-seq, '" << CONFIG_BRANCH << "' branch" << std::endl;
	std::cout << "c revision " << CONFIG_REVISION
#ifdef CONFIG_DIRTY
		<< "*"
#endif
		<< std::endl;
	std::cout << "c '" << CONFIG_TARGET << "' build from " << CONFIG_DATE << " utc" << std::endl;
	
	std::cout << "c args:";
	for(int i = 0; i < argc; i++)
		std::cout << ' ' << argv[i];
	std::cout << std::endl;

	OurConfig &config = *the_config;
	config.opts.general.verbose = 1;

	if(signal(SIGINT, onInterrupt) == SIG_ERR
			|| signal(SIGXCPU, onInterrupt) == SIG_ERR)
		throw std::runtime_error("Could not install signal handler");
	
	// parse the parameters given to the solver
	std::vector<std::string> args;
	for(int i = 1; i < argc; ++i)
		args.push_back(std::string(argv[i]));

	bool show_model = false;

	std::string instance;
	std::string model_file;
	std::vector<int> assumptions;
	for(auto i = args.begin(); i != args.end(); /* no increment here */) {
		if(*i == "-v") {
			config.opts.general.verbose = 3;
			++i;
		}else if(*i == "-budget") {
			++i;
			if(i == args.end()) {
				std::cout << "Expected argument for -budget" << std::endl;
				return 0;
			}
			config.opts.general.budget = std::atoi((*i).c_str())
					* 1000LL * 1000LL * 1000LL;
			++i;
		}else if(*i == "-timeout") {
			++i;
			if(i == args.end()) {
				std::cout << "Expected argument for -timeout" << std::endl;
				return 0;
			}
			config.opts.general.budget = std::atoi((*i).c_str())
					* 1000LL * 1000LL * 1000LL;
			config.opts.general.timeout = std::atoi((*i).c_str())
					* 1000LL * 1000LL * 1000LL;
			++i;
		}else if(*i == "-seed") {
			++i;
			if(i == args.end()) {
				std::cout << "Expected argument for -seed" << std::endl;
				return 0;
			}
			config.seedRandomEngine(std::atoi((*i).c_str()));
			++i;
		}else if(*i == "-assume") {
			++i;
			if(i == args.end()) {
				std::cout << "Expected argument for -assume" << std::endl;
				return 0;
			}
			assumptions.push_back(std::stoi((*i)));
			++i;
		}else if(*i == "-show-model") {
			show_model = true;
			++i;
		}else if(*i == "-save-model") {
			++i;
			if(i == args.end()) {
				std::cout << "Expected argument for -save_model" << std::endl;
				return 0;
			}
			model_file = *i;
			++i;
		}else if(*i == "-drat-proof") {
			config.opts.general.outputDratProof = true;
			++i;
		}else if((*i).at(0) == '-') {
			std::cout << "Illegal command line parameter '" << (*i) << "'" << std::endl;
			return 0;
		}else{
			// the last parameter is always the instance name
			instance = *i;
			++i;
			if(i != args.end()) {
				std::cout << "Unexpected parameter '" << *i
						<< "' after instance name" << std::endl;
				return 0;
			}
		}
	}

	SYS_ASSERT(SYS_ASRT_GENERAL, instance.length() > 0);

#ifdef FEATURE_GOOGLE_PROFILE
	char profile[256];
	sprintf(profile, "%d.profile", getpid());
	printf("Writing profile %s\n", profile);
	ProfilerStart(profile);
#endif
	
	int instance_fd = open(instance.c_str(), O_RDONLY);
	if(instance_fd == -1)
		throw std::runtime_error("Could not read input file");

	CnfReadHooks read_hooks(config);
	CnfParser<CnfReadHooks> reader(read_hooks, instance_fd);
	reader.parse();
	config.inputFinish();
	
	if(close(instance_fd) != 0)
		throw std::runtime_error("Could not close instance file");

	std::cout << "c parse time: " << sysGetCpuTime() << " ms" << std::endl;
	std::cout << "c parse memory: " << sysPeakMemory() << " kb" << std::endl;

	for(auto it = assumptions.begin(); it != assumptions.end(); ++it) {
		OurConfig::Literal literal = OurConfig::Literal::fromNumber(*it);
		config.lockVariable(literal.variable());
		config.assumptionEnable(literal);
	}

	satuzk::SolveState result = solve(config);

	if(model_file.length() > 0) {
		std::fstream model_stream(model_file,
				std::fstream::out | std::fstream::trunc);
		if(result == satuzk::kStateSatisfied) {
			model_stream << "SAT\n";
			
			std::vector<Bool3> model;
			model.resize(read_hooks.numVariables() + 1, undefined3());
			config.p_extModelConfig.buildModel(config, model);
			
			for(long i = 1; i <= read_hooks.numVariables(); ++i) {
				if(i != 1)
					model_stream << " ";
				// TODO: don't include non-assigned variables?
				if(model[i].isTrue()) {
					model_stream << i;
				}else model_stream << -i;
			}
			model_stream << " 0\n";
		}else if(result == satuzk::kStateUnsatisfiable) {
			model_stream << "UNSAT\n";
		}else model_stream << "INDET\n";
		model_stream.close();
	}else{
		if(result == satuzk::kStateSatisfied) {
			std::cout << "s SATISFIABLE" << std::endl;

			std::vector<Bool3> model;
			model.resize(read_hooks.numVariables() + 1, undefined3());
			config.p_extModelConfig.buildModel(config, model);
			
			if(show_model) {
				std::cout << "v";
				bool free_variables = false;
				for(long i = 1; i <= read_hooks.numVariables(); ++i) {
					// TODO: don't include non-assigned variables?
					if(model[i].isTrue()) {
						std::cout << " " << i;
					}else if(model[i].isFalse()) {
						std::cout << " " << -i;
					}else{
						free_variables = true;
					}
				}
				std::cout << " 0" << std::endl;
				if(free_variables)
					std::cout << "c There were free variables!" << std::endl;
			}
		}else if(result == satuzk::kStateUnsatisfiable) {
			std::cout << "s UNSATISFIABLE" << std::endl;
		}else if(result == satuzk::kStateAssumptionFail) {
			std::cout << "s ASSUMPTIONFAIL" << std::endl;
		}else std::cout << "s UNKNOWN" << std::endl;
	}

#ifdef FEATURE_GOOGLE_PROFILE
	ProfilerStop();	
#endif

	unsigned int secs = sysGetCpuTime() / 1000;

	std::cout << std::setprecision(2);
	std::cout << "c ------ statistics: general ------" << std::endl;
	std::cout << "c cpu time: " << secs << " secs" << std::endl;
	std::cout << "c peak memory: " << sysPeakMemory() << " kb" << std::endl;
	std::cout << "c    [      ] clause reallocations: " << config.stat.general.clauseReallocs
		<< ", collections: " << config.stat.general.clauseCollects << std::endl;

	std::cout << "c ------ search ------" << std::endl;
	std::cout << "c    conflicts: " << config.conflictNum;
	if(secs != 0)
		std::cout << ", conflicts/sec: " << (config.conflictNum / secs);
	std::cout << std::endl;
	std::cout << "c    propagations: " << config.stat.search.propagations;
	if(secs != 0)
		std::cout << ", propagations/sec: " << (config.stat.search.propagations / secs);
	std::cout << std::endl;
	std::cout << "c    deleted clauses: " << config.stat.general.deletedClauses << std::endl;
	std::cout << "c    restarts: " << config.stat.search.restarts << std::endl;
	std::cout << "c    learned units: " << config.stat.search.learnedUnits << ", binary: " << config.stat.search.learnedBinary << std::endl;
	std::cout << "c    minimized literals: " << config.stat.search.minimizedLits
			<< " of " << config.stat.search.learnedLits << ", " << (100.0f - config.stat.search.minimizedLits * 100.0f
			/ config.stat.search.learnedLits) << "% redundant" << std::endl;
	std::cout << "c    facts removed: clauses: " << config.stat.search.factElimClauses
			<< ", literals: " << config.stat.search.factElimLiterals
			<< ", runs: " << config.stat.search.factElimRuns << std::endl;
	std::cout << "c    clause reductions: " << config.stat.clauseRed.reductionRuns
		<< ", deletions: " << (100.0f * config.stat.clauseRed.clauseDeletions
			/ config.stat.clauseRed.clausesConsidered) << "%"
		<< ", freezes: " << (100.0f * config.stat.clauseRed.clauseFreezes
			/ config.stat.clauseRed.clausesConsidered) << "%"
		<< ", unfreezes: " << (100.0f * config.stat.clauseRed.clauseUnfreezes
			/ config.stat.clauseRed.clausesConsidered) << "%"
		<< ", active: " << (100.0f * config.stat.clauseRed.clausesActive
			/ (config.stat.clauseRed.clausesActive + config.stat.clauseRed.clausesNotActive)) << "%"
		<< ", learnts limit: " << config.state.clauseRed.geomSizeLimit << std::endl;
	
	std::cout << "c ------ simplification ------" << std::endl;
	std::cout << "c    [VECD  ]  vars eliminated: " << config.stat.simp.vecdEliminated
			<< ", facts discovered: " << config.stat.simp.vecdFacts << std::endl;
	std::cout << "c    [SCC   ]  vars eliminated: " << config.stat.simp.sccVariables << std::endl;
	std::cout << "c    [SUB   ]  shortcut checks: " << config.stat.simp.selfsubShortcuts
		<< ", subsumption checks: " << config.stat.simp.selfsubSubChecks
		<< ", resolution checks: " << config.stat.simp.selfsubResChecks << std::endl;
	std::cout << "c    [SUB   ]  clauses removed: " << config.stat.simp.subsumed_clauses
		<< ", clauses shortened: " << config.stat.simp.selfsubClauses
		<< ", facts discovered: " << config.stat.simp.selfsubFacts << std::endl;
	std::cout << "c    [BCE   ]  blocked clauses: " << config.stat.simp.bceEliminated << std::endl;
	std::cout << "c    [DIST  ]  checks: " << config.stat.simp.distChecks
		<< ", conflicts: " << config.stat.simp.distConflicts
		<< " (removed " << config.stat.simp.distConflictsRemoved << " lits)"
		<< ", asserts: " << config.stat.simp.distAsserts
		<< " (removed " << config.stat.simp.distAssertsRemoved << " lits)"
		<< ", ssubs: " << config.stat.simp.distSelfSubs
		<< " (removed " << config.stat.simp.distSelfSubsRemoved << " lits)" << std::endl;
	std::cout << "c    [UNHIDE]  transitive edges: " << config.stat.simp.unhideTransitiveEdges
		<< ", failed literals: " << config.stat.simp.unhideFailedLiterals
		<< ", hidden literals: " << config.stat.simp.unhideHleLiterals
		<< ", hidden tautologies: " << config.stat.simp.unhideHteClauses << std::endl;
	
	std::cout << "c    [      ]  space used for saved infos: "
			<< (config.p_extModelConfig.p_dataStack.getOffset() / 1024) << " kb" << std::endl;

	std::cout << "c ------ time ------" << std::endl;
	std::cout << "c    [      ]  inprocessing: "
			<< (config.perf.inprocTime / (1000 * 1000)) << " ms" << std::endl;
	std::cout << "c    [      ]  preprocessing: "
			<< (config.perf.preprocTime / (1000 * 1000)) << " ms" << std::endl;

	delete the_config;

	if(result == satuzk::kStateSatisfied) {
		return 10;
	}else if(result == satuzk::kStateUnsatisfiable) {
		return 20;
	}
	return 0;
}

