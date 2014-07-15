
SolverThread::ReducerLink::ReducerLink(ReducerThread &reducer) : p_reducer(reducer),
		p_reducedConsumer(reducer.newReducedConsumer()) {
}
SolverThread::ReducerLink::ReducerLink(ReducerLink &&other) : p_reducer(other.p_reducer),
		p_reducedConsumer(std::move(other.p_reducedConsumer)) {
}

void SolverThread::checkReduced() {
	for(auto it = p_reducerLinks.begin(); it != p_reducerLinks.end(); ++it) {
		while(it->p_reducedConsumer.receive()) {
			p_idleRound = false;
			
			auto length = it->p_reducedConsumer.read<int>();
			auto lbd = it->p_reducedConsumer.read<int>();
			std::vector<SolverConfig::Literal> literals;
			for(int i = 0; i < length; i++) {
				auto literal = SolverConfig::Literal::fromIndex(it->p_reducedConsumer.read<BaseDefs::LiteralIndex>());
				literals.push_back(literal);
			}
			
			p_config.reset();
			SolverConfig::Clause clause = p_config.allocClause(length, literals.begin(), literals.end());
			p_config.clauseSetLbd(clause, lbd);
			p_config.quickFreezeClause(clause);
			p_config.start();
			stat.imported++;
		}
	}
}

void SolverThread::checkCommand() {
	while(p_commandConsumer.receive()) {
		p_idleRound = false;
		
		auto tag = p_commandConsumer.read<CommandTag>();
		if(tag == CommandTag::kExit) {
			p_threadExitFlag = true;
		}else if(tag == CommandTag::kProblemDef) {
			auto num_vars = p_commandConsumer.read<long>();
			
			p_config.varReserve(num_vars);
			for(long i = 0; i < num_vars; ++i)	
				p_config.varAlloc();
		}else if(tag == CommandTag::kClause) {
			auto length = p_commandConsumer.read<int>();

			std::vector<SolverConfig::Literal> clause;
			for(int i = 0; i < length; i++) {
				auto literal = SolverConfig::Literal::fromIndex(p_commandConsumer.read<BaseDefs::LiteralIndex>());
				clause.push_back(literal);
			}
			
			p_config.inputClause(length, clause.begin(), clause.end());
			p_config.inputFinish();
		}else if(tag == CommandTag::kInitialize) {
			p_config.randomizeVsids();
			std::cout << "c initialize time: " << sysGetCpuTime() << " ms" << std::endl;
			progressHeader(p_config);
		}else if(tag == CommandTag::kContinue) {
			p_config.reset();
			p_config.start();

			p_solveActive = true;
		}else if(tag == CommandTag::kAssume) {
			auto literal = SolverConfig::Literal::fromIndex(p_commandConsumer.read<BaseDefs::LiteralIndex>());
			
			p_config.reset();
			p_config.assumptionEnable(literal);
			p_config.start();
		}else if(tag == CommandTag::kUnassume) {
			auto variable = SolverConfig::Variable::fromIndex(p_commandConsumer.read<BaseDefs::LiteralIndex>());

			//FIXME: rewrite this to work on literals and not variables
			auto lit0 = variable.zeroLiteral();
			auto lit1 = variable.oneLiteral();
			if(p_config.isAssumed(lit0)) {
				p_config.reset();
				p_config.assumptionDisable(lit0);
				p_config.start();
			}else if(p_config.isAssumed(lit1)) {
				p_config.reset();
				p_config.assumptionDisable(lit1);
				p_config.start();
			}else SYS_CRITICAL("Variable is not assumed");
		}else SYS_CRITICAL("Illegal solver command");
	}
}

void SolverThread::doWork() {
	if(!p_solveActive)
		return;
	p_idleRound = false;
	
	satuzk::TotalSearchStats search_total_stats;
	satuzk::SolveState result = satuzk::SolveState::kStateUnknown;
	search(p_config, result, search_total_stats);

	if(result == satuzk::kStateSatisfied) {
		p_messageProducer.write(MessageTag::kSolvedSat);

		p_messageProducer.write<int>(p_config.numVariables());
		for(auto var = p_config.varsBegin(); var != p_config.varsEnd(); ++var)
			p_messageProducer.write<bool>(p_config.litTrue((*var).oneLiteral()));

		p_messageProducer.send();
		p_solveActive = false;
	}else if(result == satuzk::kStateUnsatisfiable) {
		p_messageProducer.write(MessageTag::kSolvedUnsat);
		p_messageProducer.send();
		p_solveActive = false;
	}else if(result == satuzk::kStateAssumptionFail) {
		p_messageProducer.write(MessageTag::kAssumptionFail);
		p_messageProducer.send();
		p_solveActive = false;
	}else if(result == satuzk::kStateBreak) {
		sys::HptCounter now = sys::hptCurrent();
		if(now - p_lastMessage > 5LL * 1000 * 1000 * 1000) {
			satuzk::progressMessage(p_config, now);
			p_lastMessage = now;
		}
	}else SYS_CRITICAL("Illegal solve state");
}

void SolverThread::run() {
	p_lastMessage = sys::hptCurrent();

	while(!p_threadExitFlag) {
		p_idleRound = true;
		checkReduced();
		checkCommand();
		doWork();
		if(p_idleRound)
			std::this_thread::yield();
	}
}

void SolverHooks::onLearnedClause(satuzk::ClauseType<BaseDefs> clause) {
	int lbd = p_instance.p_config.clauseGetLbd(clause);
	if(lbd <= 8) {
		int length = p_instance.p_config.clauseLength(clause);
		p_instance.p_learnedProducer.write<int>(length);
		p_instance.p_learnedProducer.write<int>(lbd);
		for(auto it = p_instance.p_config.clauseBegin(clause);
				it != p_instance.p_config.clauseEnd(clause); ++it)
			p_instance.p_learnedProducer.write(*it);
		p_instance.p_learnedProducer.send();
		p_instance.stat.exported++;
	}
}

