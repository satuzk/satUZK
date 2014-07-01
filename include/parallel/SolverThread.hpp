
class SolverThread;
class ReducerThread;

class SolverHooks {
public:
	SolverHooks(SolverThread &instance) : p_instance(instance) { }
	
	void onLearnedClause(satuzk::ClauseType<BaseDefs> clause);
	
private:
	SolverThread &p_instance;
};

class SolverThread {
friend class SolverHooks;
public:
	enum class CommandTag {
		kNone, kExit, kProblemDef, kClause, kInitialize, kContinue, kAssume, kUnassume
	};

	enum class MessageTag {
		kNone, kSolvedSat, kSolvedUnsat, kAssumptionFail
	};
	
	SolverThread(int config_id)
			: p_commandProducer(p_commandPipe), p_commandConsumer(p_commandPipe),
			p_messageProducer(p_messagePipe), p_messageConsumer(p_messagePipe),
			p_learnedProducer(p_learnedPipe),
			p_config(SolverHooks(*this), config_id),
			p_solveActive(false), p_threadExitFlag(false) {
		p_config.seedRandomEngine(config_id);
	}
	SolverThread(const SolverThread &other) = delete;
	SolverThread &operator= (const SolverThread &other) = delete;

	int getConfigId() { return p_config.getConfigId(); }

	void checkReduced();
	void checkCommand();
	void doWork();
	void run();

	void linkReducer(ReducerThread &reducer) {
		p_reducerLinks.emplace_back(reducer);
	}

	util::concurrent::SingleProducerPipe<>::Consumer newLearnedConsumer() {
		return util::concurrent::SingleProducerPipe<>::Consumer(p_learnedPipe);
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

private:
	struct ReducerLink {
		ReducerLink(ReducerThread &reducer);
		ReducerLink(ReducerLink &&other);
		ReducerLink(const ReducerLink &other) = delete;
		ReducerLink &operator= (const ReducerLink &other) = delete;

		ReducerThread &p_reducer;
		util::concurrent::SingleProducerPipe<>::Consumer p_reducedConsumer;
	};
	
	util::concurrent::SingleProducerPipe<> p_commandPipe;
	util::concurrent::SingleProducerPipe<>::Producer p_commandProducer;
	util::concurrent::SingleProducerPipe<>::Consumer p_commandConsumer;

	util::concurrent::SingleProducerPipe<> p_messagePipe;
	util::concurrent::SingleProducerPipe<>::Producer p_messageProducer;
	util::concurrent::SingleProducerPipe<>::Consumer p_messageConsumer;
	
	util::concurrent::SingleProducerPipe<> p_learnedPipe;
	util::concurrent::SingleProducerPipe<>::Producer p_learnedProducer;

	std::vector<ReducerLink> p_reducerLinks;	

	typedef satuzk::Config<BaseDefs, SolverHooks> SolverConfig;
	SolverConfig p_config;
	sys::HptCounter p_lastMessage;
	bool p_solveActive;
	bool p_idleRound;
	bool p_threadExitFlag;
	
public:
	struct Stat {
		uint64_t exported;
		uint64_t imported;
		
		Stat() : exported(0), imported(0) { }
	} stat;
};

