
#include <unistd.h>
#include <algorithm>
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
#include <thread>

#include "../Config.hpp"

#include "../include/util/concurrent/SingleProducerPipe.hpp"

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
#include "../inline/simplify/Distillation.hpp"
#include "../inline/Dimacs.hpp"
#include "../include/Config.hpp"
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

volatile bool globalExitFlag = false;

#include "../include/parallel/SolverThread.hpp"
#include "../include/parallel/ReducerThread.hpp"
#include "../include/parallel/Master.hpp"
#include "../inline/parallel/SolverThread.hpp"
#include "../inline/parallel/ReducerThread.hpp"
#include "../inline/parallel/Master.hpp"

void onInterrupt(int sig) {
	std::cout << "c signal: ";
	switch(sig) {
	case SIGINT: std::cout << "SIGINT"; break;
	case SIGXCPU: std::cout << "SIGXCPU"; break;
	default: std::cout << sig;
	}
	std::cout << std::endl;
	globalExitFlag = true;
}

int main(int argc, char **argv) {
	std::cout << "c this is satUZK-par, '" << CONFIG_BRANCH << "' branch" << std::endl;
	std::cout << "c revision " << CONFIG_REVISION
#ifdef CONFIG_DIRTY
		<< "*"
#endif
		<< std::endl;
	std::cout << "c '" << CONFIG_TARGET << "' build from " << CONFIG_DATE << " utc" << std::endl;

	if(signal(SIGINT, onInterrupt) == SIG_ERR
			|| signal(SIGXCPU, onInterrupt) == SIG_ERR)
		throw std::runtime_error("Could not install signal handler");

	/* parse the parameters given to the solver */
	std::vector<std::string> args;
	for(int i = 1; i < argc; ++i)
		args.push_back(std::string(argv[i]));

	bool show_model = false;

	std::string instance;
	std::string model_file;
	for(auto i = args.begin(); i != args.end(); /* no increment here */) {
		if(*i == "-show-model") {
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
		}else if((*i).at(0) == '-') {
			std::cout << "Illegal command line parameter '" << (*i) << "'" << std::endl;
			return 0;
		}else{
			/* the last parameter is always the instance name */
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

	auto start_time = sysGetWallTime();

#ifdef FEATURE_GOOGLE_PROFILE
	char profile[256];
	sprintf(profile, "%d.profile", getpid());
	printf("Writing profile %s\n", profile);
	ProfilerStart(profile);
#endif

	Master master(instance);
	int exit_code = master.run();

	if(show_model) {
		if(exit_code == 10) {
			std::cout << "v";
			for(auto it = master.beginModel(); it != master.endModel(); ++it)
				std::cout << " " << *it;
			std::cout << " 0" << std::endl;
		}
	}
	
	if(model_file.length() > 0) {
		std::fstream model_stream(model_file,
				std::fstream::out | std::fstream::trunc);
		if(exit_code == 10) {
			model_stream << "SAT\n";
			
			for(auto it = master.beginModel(); it != master.endModel(); ++it) {
				if(it != master.beginModel())
					model_stream << " ";
				model_stream << *it;
			}
			model_stream << " 0\n";
		}else if(exit_code == 20) {
			model_stream << "UNSAT\n";
		}else model_stream << "INDET\n";
		model_stream.close();
	}

#ifdef FEATURE_GOOGLE_PROFILE
	ProfilerStop();	
#endif

	unsigned int secs = (sysGetWallTime() - start_time) / 1000;
	std::cout << "c wall time: " << secs << " secs" << std::endl;
	return exit_code;
}

