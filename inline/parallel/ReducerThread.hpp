
void ReducerThread::checkCommand() {
	while(p_commandConsumer.receive()) {
		p_idleRound = false;
		
		auto tag = p_commandConsumer.read<CommandTag>();
		if(tag == CommandTag::kExit) {
			p_threadExitFlag = true;
		}else if(tag == CommandTag::kProblemDef) {
			auto num_vars = p_commandConsumer.read<long>();
				
			p_config.varReserve(num_vars);
			for(long i = 0; i < num_vars; ++i) {
				p_config.varAlloc();
				p_distConfig.onAllocVariable();
			}
		}else if(tag == CommandTag::kClause) {
			auto length = p_commandConsumer.read<int>();
			std::vector<ReducerConfig::Literal> literals;
			for(int i = 0; i < length; i++) {
				auto literal = ReducerConfig::Literal::fromIndex(p_commandConsumer.read<BaseDefs::LiteralIndex>());
				literals.push_back(literal);
			}
			
			p_config.reset();
			p_config.inputClause(length, literals.begin(), literals.end());
			p_config.inputFinish();
		}else SYS_CRITICAL("Illegal reducer command");
	}
}

void ReducerThread::checkLearned() {
	const unsigned int kQueueSize = 1000;
	
	for(auto it = p_solverLinks.begin(); it != p_solverLinks.end(); ++it) {
		while(it->p_learnedConsumer.receive()) {
			p_idleRound = false;
			
			auto length = it->p_learnedConsumer.read<int>();
			auto lbd = it->p_learnedConsumer.read<int>();
			std::vector<ReducerConfig::Literal> literals;
			for(int i = 0; i < length; i++) {
				auto literal = ReducerConfig::Literal::fromIndex(it->p_learnedConsumer.read<BaseDefs::LiteralIndex>());
				literals.push_back(literal);
			}

			bool import = true;
			if(p_workQueue.size() >= kQueueSize)
				if(lbd >= p_workQueue.top().lbd)
					import = false;
			
			if(import) {
				ReducerConfig::Clause new_clause = p_config.allocClause(length,
						literals.begin(), literals.end());
				p_config.clauseSetLbd(new_clause, lbd);
				
				// NOTE: the queue is a max-priority-queue. this guarantees
				// that we always keep the items with lowest lbd value, but we
				// process them in reverse order, i.e. highest lbd first
				p_workQueue.push(WorkQueueItem(new_clause, lbd));
				if(p_workQueue.size() > kQueueSize) {
					ReducerConfig::Clause top_clause = p_workQueue.top().clause;
					p_config.deleteClause(top_clause);
					p_workQueue.pop();
				}
			}
		}
	}
}

void ReducerThread::doWork() {
	class DistHooks {
	public:
		DistHooks(ReducerThread &reducer) : p_reducer(reducer) {
		}

		typedef ReducerConfig::Literal Literal;
		typedef ReducerConfig::Variable Variable;
		typedef ReducerConfig::Clause Clause;
		typedef ReducerConfig::ClauseLitIndex ClauseLitIndex;
		typedef ReducerConfig::Antecedent Antecedent;
		typedef ReducerConfig::Activity Activity;
		typedef ReducerConfig::ClauseLitIterator ClauseLitIterator;
		typedef ReducerConfig::AntecedentIterator AntecedentIterator;
		typedef ReducerConfig::ConflictIterator ConflictIterator;

		Activity varGetActivity(Variable var) { return p_reducer.p_config.varGetActivity(var); }
		Antecedent varAntecedent(Variable var) { return p_reducer.p_config.varAntecedent(var); }
		bool varIsLocked(Variable var) { return p_reducer.p_config.varIsLocked(var); }
		bool litTrue(Literal lit) { return p_reducer.p_config.litTrue(lit); }
		bool litFalse(Literal lit) { return p_reducer.p_config.litFalse(lit); }
		bool varIsFixed(Variable var) { return p_reducer.p_config.varIsFixed(var); }
		ClauseLitIndex clauseLength(Clause clause) { return p_reducer.p_config.clauseLength(clause); }
		ClauseLitIterator clauseBegin(Clause clause) { return p_reducer.p_config.clauseBegin(clause); }
		ClauseLitIterator clauseEnd(Clause clause) { return p_reducer.p_config.clauseEnd(clause); }
		AntecedentIterator causesBegin(Antecedent antecedent) { return p_reducer.p_config.causesBegin(antecedent); }
		AntecedentIterator causesEnd(Antecedent antecedent) { return p_reducer.p_config.causesEnd(antecedent); }
		void reset() { p_reducer.p_config.reset(); }
		void start() { p_reducer.p_config.start(); }
		void pushLevel() { p_reducer.p_config.pushLevel(); }
		void pushAssign(Literal lit, Antecedent antecedent)
			{ p_reducer.p_config.pushAssign(lit, antecedent); }
		void propagate() { p_reducer.p_config.propagate(); }
		bool atConflict() { return p_reducer.p_config.atConflict(); }
		bool isResolveable() { return p_reducer.p_config.isResolveable(); }
		ConflictIterator conflictBegin() { return p_reducer.p_config.conflictBegin(); }
		ConflictIterator conflictEnd() { return p_reducer.p_config.conflictEnd(); }
		void resolveConflict() { p_reducer.p_config.resolveConflict(); }

		struct {
			struct {
				uint64_t distChecks;
				uint64_t distConflicts;
				uint64_t distConflictsRemoved;
				uint64_t distAsserts;
				uint64_t distAssertsRemoved;
				uint64_t distSelfSubs;
				uint64_t distSelfSubsRemoved;
			} simp;
		} stat;
	private:
		ReducerThread &p_reducer;
	};

	if(p_config.kReportReducerSample) {
		p_config.report<ReducerConfig::ReportTag>(ReducerConfig::ReportTag::kReducerSample);
		p_config.report<uint64_t>(sys::hptCurrent());
		p_config.report<uint32_t>(p_workQueue.size());
	}
	
	DistHooks dist_hooks(*this);

	int num_runs = 0;
	while(!p_workQueue.empty() && num_runs < 1000) {
		p_idleRound = false;
		num_runs++;

		ReducerConfig::Clause clause = p_workQueue.top().clause;
		p_workQueue.pop();
		if(p_config.clauseLength(clause) == 1) { // TODO: replace this by lbd == 1
			p_reducedProducer.write<int>(p_config.clauseLength(clause));
			p_reducedProducer.write<int>(1);
			for(auto cp = p_config.clauseBegin(clause); cp != p_config.clauseEnd(clause); ++cp)
				p_reducedProducer.write<BaseDefs::LiteralIndex>((*cp).getIndex());
			p_reducedProducer.send();
		}else{
			DistResult result = p_distConfig.distill(dist_hooks, clause, p_config.clauseGetFirst(clause));
			if(result == DistResult::kDistilled) {
				p_reducedProducer.write<int>(p_distConfig.newLiterals.size());
				p_reducedProducer.write<int>(p_distConfig.newLiterals.size());
				for(auto cp = p_distConfig.newLiterals.begin(); //FIXME: don't use private members directly
						cp != p_distConfig.newLiterals.end(); ++cp)
					p_reducedProducer.write<BaseDefs::LiteralIndex>((*cp).getIndex());
				p_reducedProducer.send();
				
				p_config.reset();
				p_config.deleteClause(clause);

				ReducerConfig::Clause new_clause = p_config.allocClause(p_distConfig.newLiterals.size(),
						p_distConfig.newLiterals.begin(), p_distConfig.newLiterals.end());
				p_config.installClause(new_clause);
				
				p_redundantCount++;
			}else if(result == DistResult::kMinimal) {
				p_reducedProducer.write<int>(p_config.clauseLength(clause));
				p_reducedProducer.write<int>(p_config.clauseGetLbd(clause));
				for(auto cp = p_config.clauseBegin(clause); cp != p_config.clauseEnd(clause); ++cp)
					p_reducedProducer.write<BaseDefs::LiteralIndex>((*cp).getIndex());
				p_reducedProducer.send();
				
				p_config.reset();
				p_config.installClause(clause);
			}
			p_distCount++;
			p_distConfig.reset();
		}
	}
}

void ReducerThread::run() {
	while(!p_threadExitFlag) {
		p_idleRound = true;
		checkLearned();
		checkCommand();
		doWork();
		if(p_idleRound)
			std::this_thread::yield();
	}
	std::cout << std::setprecision(2);
	std::cout << "c redundant: " << (100.0f * p_redundantCount / p_distCount) << "%" << std::endl;
}

void ReducerHooks::onLearnedClause(satuzk::ClauseType<BaseDefs> clause) {
	int length = p_instance.p_config.clauseLength(clause);
	int lbd = p_instance.p_config.clauseGetLbd(clause);
	p_instance.p_reducedProducer.write<int>(length);
	p_instance.p_reducedProducer.write<int>(lbd);
	for(auto it = p_instance.p_config.clauseBegin(clause);
			it != p_instance.p_config.clauseEnd(clause); ++it)
		p_instance.p_reducedProducer.write(*it);
	p_instance.p_reducedProducer.send();
}

