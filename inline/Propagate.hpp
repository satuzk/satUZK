
namespace satuzk {

template<typename BaseDefs>
class PropagateConfigStruct {
public:
	typedef satuzk::LiteralType<BaseDefs> Literal;
	typedef typename BaseDefs::Declevel Declevel;
	typedef typename BaseDefs::Order Order;

	struct DecisionInfo {
		// index of the first assignment at this decision level
		Order firstIndex;
	};

	PropagateConfigStruct() : p_propagatePointer(0) { }

	Declevel curDeclevel() {
		return p_decisions.size();
	}
	
	Order currentOrder() {
		return p_assigns.size();
	}
	Order propagatePointer() {
		return p_propagatePointer;
	}

	void newDecision() {
		// PRE-CONDITION: no assignments have been pushed but not propagated yet
		SYS_ASSERT(SYS_ASRT_GENERAL, !propagatePending());

		DecisionInfo decision;
		decision.firstIndex = p_assigns.size();
		p_decisions.push_back(decision);
	}
	DecisionInfo &peekDecision() {
		return p_decisions.back();
	}
	DecisionInfo popDecision() {
		// PRE-CONDITION: the assign stack contains no assigns
		// that were pushed after the current decision level
		DecisionInfo info = p_decisions.back();
		SYS_ASSERT(SYS_ASRT_GENERAL, info.firstIndex == p_assigns.size());
		SYS_ASSERT(SYS_ASRT_GENERAL, info.firstIndex == p_propagatePointer);
		
		p_decisions.pop_back();
		return info;
	}

	void pushAssign(Literal literal) {
		p_assigns.push_back(literal);
	}
	Literal getAssignByOrder(Order order) {
		return p_assigns[order];
	}
	Literal peekAssign() {
		return p_assigns.back();
	}
	Literal popAssign() {
		/* PRE-CONDITION: the top-most literal on the assign stack
				is from the current decision level */
		DecisionInfo info = p_decisions.back();
		SYS_ASSERT(SYS_ASRT_GENERAL, p_assigns.size() > p_propagatePointer);
		SYS_ASSERT(SYS_ASRT_GENERAL, p_assigns.size() > info.firstIndex);
		
		Literal literal = p_assigns.back();
		p_assigns.pop_back();
		return literal;
	}
	unsigned int assignsAtLevel(Declevel declevel) {
		SYS_ASSERT(SYS_ASRT_GENERAL, declevel > 0);
		if(declevel == curDeclevel())
			return p_assigns.size()
				- p_decisions[declevel - 1].firstIndex;
		return p_decisions[declevel].firstIndex
				- p_decisions[declevel - 1].firstIndex;
	}
	unsigned int firstAssignAtLevel(Declevel declevel) {
		return p_decisions[declevel - 1].firstIndex;
	}
	unsigned int lastAssignAtLevel(Declevel declevel) {
		if(declevel == curDeclevel())
			return p_assigns.size();
		return p_decisions[declevel].firstIndex;
	}

	void propagateReset() {
		DecisionInfo info = p_decisions.back();
		p_propagatePointer = info.firstIndex;
	}
	bool propagatePending() {
		return p_propagatePointer != p_assigns.size();
	}
	Literal nextPropagation() {
		Literal literal = p_assigns[p_propagatePointer];
		p_propagatePointer++;
		return literal;
	}

private:
	std::vector<Literal> p_assigns;
	std::vector<DecisionInfo> p_decisions;
	Order p_propagatePointer;
};

template<typename Hooks, typename Callback>
__attribute__((always_inline)) inline bool propagateWatch(Hooks &hooks, Callback &callback,
		typename Hooks::Literal literal) {
	auto begin = hooks.watchBegin(literal);
	auto end = hooks.watchEnd(literal);
	auto wp = begin;
	for(auto rp = begin; rp != end; ++rp) {
		if((*rp).isBinary()) {
			// if the literal is already true there is nothing to do
			typename Hooks::Literal implied = (*rp).binaryGetImplied();
			SYS_ASSERT(SYS_ASRT_GENERAL, hooks.varIsPresent(implied.variable()));
			if(hooks.litTrue(implied)) {
				*wp = *rp;
				++wp;
				continue;
			}

			// otherwise there might be a conflict
			if(hooks.litFalse(implied)) {
				auto var = implied.variable();
				auto declevel = hooks.varDeclevel(var);
				SYS_ASSERT(SYS_ASRT_GENERAL, declevel == hooks.curDeclevel());
				hooks.raiseConflict(Hooks::Conflict::makeBinary(literal, implied.inverse()));
				
				// copy the remaining watch list
				while(rp != end) {
					*wp = *rp;
					++rp; ++wp;
				}
				hooks.watchCut(literal, wp);
				return true;
			}
			
			hooks.pushAssign(implied, Hooks::Antecedent::makeBinary(literal));
			callback.onUnit(implied);
			
			*wp = *rp;
			++wp;
			continue;
		}
		
		// try to avoid inspecting the clause
		auto blocking = (*rp).longGetBlocking();
		auto clause = (*rp).longGetClause();
		if(hooks.litTrue(blocking)) {
			(*wp).setLong();
			(*wp).longSetClause(clause);
			(*wp).longSetBlocking(blocking);
			++wp;
			continue;
		}

		auto trigger = literal.inverse();
		auto current1 = hooks.clauseGetFirst(clause);
		auto current2 = hooks.clauseGetSecond(clause);
		SYS_ASSERT(SYS_ASRT_GENERAL, trigger == current1 || trigger == current2);

		// reorder the current literals so that the	possibly unassigned one comes first
		// write that literal to index #1
		if(current1 == trigger) {
			current1 = current2;
			current2 = trigger;
		}
		hooks.clauseSetFirst(clause, current1);
		
		// case 1: a watched literal is true
		if(hooks.litTrue(current1)) {
			hooks.clauseSetSecond(clause, current2);
			(*wp).setLong();
			(*wp).longSetClause(clause);
			(*wp).longSetBlocking(current1);
			++wp;
			continue;
		}
		auto var = current1.variable();	
		
		// try to find an unassigned literal. write that literal to index #2
		for(auto it = hooks.clauseBegin3(clause);
				it != hooks.clauseEnd(clause); ++it) {
			typename Hooks::Literal lit = *it;
			if(!hooks.litFalse(lit)) {
				// case 2: the are is a unassigned literal
				hooks.clauseSetSecond(clause, lit);
				*it = current2;
			
				// the propagated literal is no longer watched 
				hooks.watchInsertClause(lit.inverse(), current1, clause);
				goto next_clause;
			}
		} 
			
		// the propagated literal is still watched
		hooks.clauseSetSecond(clause, current2);
		(*wp).setLong();
		(*wp).longSetClause(clause);
		(*wp).longSetBlocking(current1);
		++wp;
	
		if(hooks.varAssigned(var)) {
			// case 3: the clause is unsat
			hooks.raiseConflict(Hooks::Conflict::makeClause(clause));
			
			// copy the remaining watch list, including the current entry
			++rp;
			while(rp != end) {
				*wp = *rp;
				++rp; ++wp;
			}
			hooks.watchCut(literal, wp);
			return true;
		}else{
			// case 4: the clause is unit
			hooks.pushAssign(current1, Hooks::Antecedent::makeClause(clause));
			callback.onUnit(current1);

			// update the literal-block-distance
			unsigned int cur_lbd = hooks.clauseGetLbd(clause);
			unsigned int new_lbd = computeClauseLbd(hooks, clause);
			if(new_lbd + 1 < cur_lbd) {
				hooks.p_clauseConfig.setFlagImproved(clause);
				hooks.clauseSetLbd(clause, new_lbd);
			}
		}
		next_clause:;
	}
	hooks.watchCut(literal, wp);
	return false;
}

}; // namespace satuzk

