
#include <queue>

class SolverThread;
class ReducerThread;

class ReducerHooks {
public:
	ReducerHooks(ReducerThread &instance) : p_instance(instance) { }
	
	void onLearnedClause(satuzk::ClauseType<BaseDefs> clause);
	
private:
	ReducerThread &p_instance;
};

class ReducerThread {
friend class ReducerHooks;
public:
	enum class CommandTag {
		kNone, kExit, kProblemDef, kClause, kInitialize
	};
	
	ReducerThread(int config_id) :
			p_commandProducer(p_commandPipe), p_commandConsumer(p_commandPipe),
			p_messageProducer(p_messagePipe), p_messageConsumer(p_messagePipe),
			p_reducedProducer(p_reducedPipe),
			p_config(ReducerHooks(*this), config_id), p_threadExitFlag(false), p_distCount(0), p_redundantCount(0) {
	}
	ReducerThread(const ReducerThread &other) = delete;
	ReducerThread &operator= (const ReducerThread &other) = delete;

	void linkSolver(SolverThread &solver) {
		p_solverLinks.emplace_back(solver);
	}
	
	util::concurrent::SingleProducerPipe<>::Consumer newReducedConsumer() {
		return util::concurrent::SingleProducerPipe<>::Consumer(p_reducedPipe);
	}

	template<typename T>
	void writeCommand(const T &data) {
		p_commandProducer.write(data);
	}
	void sendCommand() {
		p_commandProducer.send();
	}

	bool recvMessage() {
		return p_messageConsumer.receive();
	}
	template<typename T>
	T readMessage() {
		return p_messageConsumer.read<T>();
	}

	void checkCommand();
	void checkLearned();
	void doWork();
	void run();
	
private:
	struct SolverLink {
		SolverLink(SolverThread &solver) : p_solver(solver),
				p_learnedConsumer(solver.newLearnedConsumer()) {
		}
		SolverLink(SolverLink &&other) : p_solver(other.p_solver),
				p_learnedConsumer(std::move(other.p_learnedConsumer)) {
		}
		SolverLink(const SolverLink &other) = delete;
		SolverLink &operator= (const SolverLink &other) = delete;
		
		SolverThread &p_solver;
		util::concurrent::SingleProducerPipe<>::Consumer p_learnedConsumer;
	};
	
	util::concurrent::SingleProducerPipe<> p_commandPipe;
	util::concurrent::SingleProducerPipe<>::Producer p_commandProducer;
	util::concurrent::SingleProducerPipe<>::Consumer p_commandConsumer;

	util::concurrent::SingleProducerPipe<> p_messagePipe;
	util::concurrent::SingleProducerPipe<>::Producer p_messageProducer;
	util::concurrent::SingleProducerPipe<>::Consumer p_messageConsumer;
	
	util::concurrent::SingleProducerPipe<> p_reducedPipe;
	util::concurrent::SingleProducerPipe<>::Producer p_reducedProducer;
	std::vector<SolverLink> p_solverLinks;

	typedef satuzk::Config<BaseDefs, ReducerHooks> ReducerConfig;
	ReducerConfig p_config;

	struct DistDefs {
		typedef ReducerConfig::Clause Clause;
		typedef ReducerConfig::Literal Literal;
	};
	DistConfig<DistDefs> p_distConfig;

	struct WorkQueueItem {
		ReducerConfig::Clause clause;
		int lbd;

		WorkQueueItem(ReducerConfig::Clause clause, int lbd)
				: clause(clause), lbd(lbd) {
		}

		bool operator< (const WorkQueueItem &other) const {
			return lbd < other.lbd;
		}
	};

	std::priority_queue<WorkQueueItem> p_workQueue;

	bool p_idleRound;
	bool p_threadExitFlag;
	uint64_t p_distCount, p_redundantCount;//FIXME
};

