
int Master::run() {
	p_solvers.push_back(new SolverThread(1));
	p_solvers.push_back(new SolverThread(2));
	p_reducers.push_back(new ReducerThread(4));

	p_solvers[0]->linkReducer(*p_reducers[0]);
	p_solvers[1]->linkReducer(*p_reducers[0]);
	p_reducers[0]->linkSolver(*p_solvers[0]);
	p_reducers[0]->linkSolver(*p_solvers[1]);

	for(auto it = p_solvers.begin(); it != p_solvers.end(); ++it)
		p_threads.emplace_back(std::bind(&SolverThread::run, *it));
	for(auto it = p_reducers.begin(); it != p_reducers.end(); ++it)
		p_threads.emplace_back(std::bind(&ReducerThread::run, *it));

	int instance_fd = open(p_instance.c_str(), O_RDONLY);
	if(instance_fd == -1)
		throw std::runtime_error("Could not open instance file");
	
	CnfReadHooks read_hooks(*this);
	CnfParser<CnfReadHooks> reader(read_hooks, instance_fd);
	reader.parse();
	
	if(close(instance_fd) != 0)
		throw std::runtime_error("Could not close instance file");

	std::cout << "Finished parsing" << std::endl;

	for(auto bc = p_solvers.begin(); bc != p_solvers.end(); ++bc) {
		(*bc)->writeCommand(SolverThread::CommandTag::kInitialize);
		(*bc)->sendCommand();
		
		(*bc)->writeCommand(SolverThread::CommandTag::kContinue);
		(*bc)->sendCommand();
	}

	int exit_code = 0;

	while(!globalExitFlag) {
		for(auto it = p_solvers.begin(); it != p_solvers.end(); ++it) {
			if((*it)->recvMessage()) {
				auto tag = (*it)->readMessage<SolverThread::MessageTag>();
				if(tag == SolverThread::MessageTag::kSolvedSat) {
					std::cout << "s SATISFIABLE" << std::endl;

					int num_vars = (*it)->readMessage<int>();
					for(int i = 0; i < num_vars; i++) {
						bool is_one = (*it)->readMessage<bool>();
						int literal_num = is_one ? (i + 1) : -(i + 1);
						p_model.push_back(literal_num);
					}

					globalExitFlag = true;
					exit_code = 10;
					break;
				}else if(tag == SolverThread::MessageTag::kSolvedUnsat) {
					std::cout << "s UNSATISFIABLE" << std::endl;
					globalExitFlag = true;
					exit_code = 20;
					break;
				}else SYS_CRITICAL("Illegal solver message");
			}
		}
		if(!globalExitFlag)
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
	}

	for(auto bc = p_solvers.begin(); bc != p_solvers.end(); ++bc) {
		(*bc)->writeCommand(SolverThread::CommandTag::kExit);
		(*bc)->sendCommand();
	}
	for(auto bc = p_reducers.begin(); bc != p_reducers.end(); ++bc) {
		(*bc)->writeCommand(ReducerThread::CommandTag::kExit);
		(*bc)->sendCommand();
	}
	
	for(auto it = p_threads.begin(); it != p_threads.end(); ++it)
		(*it).join();
	
	for(auto it = p_solvers.begin(); it != p_solvers.end(); ++it)
		std::cout << "c (" << (*it)->getConfigId() << " ) exported: "
			<< (*it)->stat.exported << ", imported: " << (*it)->stat.imported << std::endl;

	for(auto it = p_solvers.begin(); it != p_solvers.end(); ++it)
		delete *it;
	for(auto it = p_reducers.begin(); it != p_reducers.end(); ++it)
		delete *it;
	return exit_code;
}

std::vector<int>::iterator Master::beginModel() {
	return p_model.begin();
}
std::vector<int>::iterator Master::endModel() {
	return p_model.end();
}

void Master::CnfReadHooks::onProblem(long num_vars, long num_clauses) {
	p_varCount = num_vars;

	for(auto bc = p_master.p_solvers.begin(); bc != p_master.p_solvers.end(); ++bc) {
		(*bc)->writeCommand(SolverThread::CommandTag::kProblemDef);
		(*bc)->writeCommand<long>(num_vars);
		(*bc)->sendCommand();
	}

	for(auto bc = p_master.p_reducers.begin(); bc != p_master.p_reducers.end(); ++bc) {
		(*bc)->writeCommand(ReducerThread::CommandTag::kProblemDef);
		(*bc)->writeCommand<long>(num_vars);
		(*bc)->sendCommand();
	}
}

void Master::CnfReadHooks::onClause(std::vector<long> &in_clause) {
	// remove duplicates and check for tautologies
	// TODO: move this to the cnf reader
	auto k = in_clause.begin();
	for(auto i = in_clause.begin(); i != in_clause.end(); ++i) {
		bool duplicate = false;
		for(auto j = i + 1; j != in_clause.end(); ++j) {
			if(*j == *i) {
				duplicate = true;
			}else if(*j == -*i) {
				return;
			}
		}
		if(duplicate)
			continue;
		*k = *i;
		k++;
	}
	in_clause.resize(k - in_clause.begin());
	
	// transform input variable ids to internal variable ids
	std::vector<BaseDefs::LiteralIndex> out_clause;
	for(auto it = in_clause.begin(); it != in_clause.end(); ++it) {
		long input_variable = (*it) < 0 ? -(*it) : (*it);
		SYS_ASSERT(SYS_ASRT_GENERAL, input_variable <= p_varCount);
		
		BaseDefs::LiteralIndex intern_variable = input_variable - 1;
		BaseDefs::LiteralIndex intern_literal = (*it) < 0
				? 2 * intern_variable : 2 * intern_variable + 1;
		out_clause.push_back(intern_literal);
	}
	
	for(auto bc = p_master.p_solvers.begin(); bc != p_master.p_solvers.end(); ++bc) {
		(*bc)->writeCommand(SolverThread::CommandTag::kClause);
		(*bc)->writeCommand<int>(out_clause.size());
		for(auto cp = out_clause.begin(); cp != out_clause.end(); ++cp)
			(*bc)->writeCommand<BaseDefs::LiteralIndex>(*cp);
		(*bc)->sendCommand();
	}
	
	for(auto bc = p_master.p_reducers.begin(); bc != p_master.p_reducers.end(); ++bc) {
		(*bc)->writeCommand(ReducerThread::CommandTag::kClause);
		(*bc)->writeCommand<int>(out_clause.size());
		for(auto cp = out_clause.begin(); cp != out_clause.end(); ++cp)
			(*bc)->writeCommand<BaseDefs::LiteralIndex>(*cp);
		(*bc)->sendCommand();
	}
}

