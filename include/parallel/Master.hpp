
class Master {
public:
	Master(const std::string &instance) : p_instance(instance) {
	}
	
	int run();

	std::vector<int>::iterator beginModel();
	std::vector<int>::iterator endModel();
	
private:
	class CnfReadHooks {
	public:
		CnfReadHooks(Master &master) : p_master(master) { }
		
		void onProblem(long num_vars, long num_clauses);
		void onClause(std::vector<long> &in_clause);

	private:
		long p_varCount;
		Master &p_master;
	};
	
	std::string p_instance;
	std::vector<int> p_model;
	std::vector<std::thread> p_threads;
	std::vector<SolverThread*> p_solvers;
	std::vector<ReducerThread*> p_reducers;
};

