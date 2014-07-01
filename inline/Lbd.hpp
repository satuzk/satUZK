
namespace satuzk {

template<typename Hooks>
int computeClauseLbd(Hooks &hooks, typename Hooks::Clause clause) {
	hooks.lbdInit();
	for(auto i = hooks.clauseBegin(clause); i != hooks.clauseEnd(clause); ++i) {
		auto var = (*i).variable();
		hooks.lbdInsert(hooks.varDeclevel(var));
	}
	for(auto i = hooks.clauseBegin(clause); i != hooks.clauseEnd(clause); ++i) {
		auto var = (*i).variable();
		hooks.lbdCleanup(hooks.varDeclevel(var));
	}
	return hooks.lbdResult();
}

}; // namespace satuzk

