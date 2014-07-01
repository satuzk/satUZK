
namespace satuzk {

template<typename BaseDefs>
class LiteralType;

template<typename BaseDefs>
class VariableType {
public:
	typedef typename BaseDefs::LiteralIndex Index;
	
	static VariableType<BaseDefs> illegalVar() {
		return VariableType<BaseDefs>(-1);
	}
	static VariableType<BaseDefs> fromIndex(Index index) {
		return VariableType<BaseDefs>(index);
	}
	
	VariableType() : p_index(-1) { }
	
	Index getIndex() {
		return p_index;
	}
	int64_t toNumber() {
		return p_index + 1;
	}
	
	bool operator== (const VariableType<BaseDefs> &other) {
		return p_index == other.p_index;
	}
	bool operator!= (const VariableType<BaseDefs> &other) {
		return p_index != other.p_index;
	}

	LiteralType<BaseDefs> zeroLiteral();
	LiteralType<BaseDefs> oneLiteral();
private:
	explicit VariableType(Index index) : p_index(index) { }
	
	Index p_index;
};

template <typename BaseDefs>
class LiteralType {
public:
	typedef typename BaseDefs::LiteralIndex Index;
	
	static LiteralType<BaseDefs> illegalLit() {
		return LiteralType<BaseDefs>(-1);
	}
	static LiteralType<BaseDefs> fromIndex(Index index) {
		return LiteralType<BaseDefs>(index);
	}

	LiteralType() : p_index(-1) { }
	
	Index getIndex() {
		return p_index;
	}
	int64_t toNumber() {
		return isOneLiteral() ? variable().toNumber() : -variable().toNumber();
	}
	
	bool operator== (const LiteralType<BaseDefs> &other) {
		return p_index == other.p_index;
	}
	bool operator!= (const LiteralType<BaseDefs> &other) {
		return p_index != other.p_index;
	}

	VariableType<BaseDefs> variable() {
		return VariableType<BaseDefs>::fromIndex(p_index >> 1);
	}
	
	bool isOneLiteral() {
		return p_index & 1;
	}
	int polarity() {
		return isOneLiteral() ? 1 : -1;
	}
	LiteralType<BaseDefs> inverse() {
		return LiteralType<BaseDefs>::fromIndex(p_index ^ 1);
	}

private:
	explicit LiteralType(Index index) : p_index(index) { }
	
	Index p_index;
};

template <typename BaseDefs>
LiteralType<BaseDefs> VariableType<BaseDefs>::zeroLiteral() {
	return LiteralType<BaseDefs>::fromIndex(p_index << 1);
}
template <typename BaseDefs>
LiteralType<BaseDefs> VariableType<BaseDefs>::oneLiteral() {
	return LiteralType<BaseDefs>::fromIndex((p_index << 1) + 1);
}

template<typename BaseDefs>
class ClauseType;

template<typename BaseDefs>
struct WatchlistEntryStruct {
	typedef LiteralType<BaseDefs> Literal;
	typedef ClauseType<BaseDefs> Clause;

	static const uint32_t kFlagMask = 0x80000000;
	static const uint32_t kFlagLong = 0;
	static const uint32_t kFlagBinary = 0x80000000;

	// stores the flags for all entries
	// stores the clause for binary and long entries
	uint32_t field1;
	// stores the implied literal for binary entries
	// stores the blocking literal for long entries
	uint32_t field2;
	
	WatchlistEntryStruct() : field1(0), field2(0) { }
	
	void setLong();
	void setBinary();
	bool isLong();
	bool isBinary();

	void binarySetImplied(Literal literal);
	void binarySetClause(Clause clause);
	Literal binaryGetImplied();
	Clause binaryGetClause();
	
	void longSetClause(Clause clause);
	void longSetBlocking(Literal literal);
	Clause longGetClause();
	Literal longGetBlocking();
};

template<typename BaseDefs>
class OcclistStruct {
public:
	typedef ClauseType<BaseDefs> Clause;

	typedef util::vectors::simple<Clause> ClauseVector;
	typedef typename ClauseVector::iterator iterator;

	OcclistStruct() : p_vector() { }
	OcclistStruct(OcclistStruct &&other)
		: p_vector(std::move(other.p_vector)) { }
	OcclistStruct(const OcclistStruct &) = delete;
	void operator= (const OcclistStruct &) = delete;

	void insert(Clause clause) {
		p_vector.push_back(clause);
	}
	void clear() {
		p_vector.resize(0);
	}
	void shrink() {
		p_vector.shrink_to_fit();
	}
	void erase(iterator iterator) {
		p_vector.erase(iterator);
	}

	typename ClauseVector::size_type size() {
		return p_vector.size();
	}
	iterator begin() {
		return p_vector.begin();
	}
	iterator end() {
		return p_vector.end();
	}
	void cutoff(iterator after) {
		p_vector.resize(p_vector.index_of(after));
	}

private:
	ClauseVector p_vector;
};

template<typename BaseDefs>
class WatchlistStruct {
public:
	typedef WatchlistEntryStruct<BaseDefs> Entry;

	typedef util::vectors::simple<Entry> EntryVector;
	typedef typename EntryVector::iterator iterator;

	WatchlistStruct() : p_vector() { }
	WatchlistStruct(WatchlistStruct &&other)
		: p_vector(std::move(other.p_vector)) { }
	WatchlistStruct(const WatchlistStruct &) = delete;
	void operator= (const WatchlistStruct &) = delete;

	typename EntryVector::size_type size() {
		return p_vector.size();
	}
	void insert(Entry entry) {
		p_vector.push_back(entry);
	}
	void erase(iterator it) {
		iterator end = p_vector.end();
		--end;
		*it = *end;
		p_vector.pop_back();
	}
	iterator begin() {
		return p_vector.begin();
	}
	iterator end() {
		return p_vector.end();
	}
	void cutoff(iterator after) {
		p_vector.resize(p_vector.index_of(after));
	}
	void clear() {
		p_vector.clear();
	}

private:
	EntryVector p_vector;
};

template<typename BaseDefs>
class VarConfigStruct {
public:
	typedef typename BaseDefs::LiteralIndex Index;
	typedef LiteralType<BaseDefs> Literal;
	typedef VariableType<BaseDefs> Variable;
	typedef ClauseType<BaseDefs> Clause;
	typedef typename BaseDefs::Declevel Declevel;
	typedef AntecedentStruct<BaseDefs> Antecedent;
	
	typedef uint8_t Flags;
	static const Flags kVarflagDeleted = 1;
	static const Flags kVarflagSavedOne = 2;
	static const Flags kVarflagModelOne = 4;
	// true if the variable must not be eliminted
	static const Flags kVarflagProtected = 8;

	static const Flags kLitflagMarked = 2;
	// true if the literal is an assumption
	// (i.e. must be set to true before all other decisions)
	static const Flags kLitflagAssumption = 4;

	class AssignInfo {
	public:
		AssignInfo() : p_value(kValueNone) { }

		void assign(bool state) {
			p_value = kAssignedFlag | (int)state;
		}
		void unassign() {
			p_value = kValueNone;
		}

		bool isOne() {
			return p_value == kValueTrue;
		}
		bool isZero() {
			return p_value == kValueFalse;
		}
		bool isAssigned() {
			return (p_value & kAssignedFlag);
		}
		bool litIsTrue(Literal literal) {
			if(!(p_value & 2))
				return false;
			return !((p_value ^ literal.getIndex()) & 1);
		}
		bool litIsFalse(Literal literal) {
			if(!(p_value & 2))
				return false;
			return (p_value ^ literal.getIndex()) & 1;
		}

	private:
		typedef uint8_t Value;
		static const Value kValueNone = 0;
		static const Value kValueTrue = 3;
		static const Value kValueFalse = 2;
		static const Value kAssignedFlag = 2;

		Value p_value;
	};

	typedef OcclistStruct<BaseDefs> Occlist;
	typedef typename Occlist::iterator OccurIterator;

	typedef WatchlistEntryStruct<BaseDefs> WatchlistEntry;
	typedef WatchlistStruct<BaseDefs> Watchlist;
	typedef typename Watchlist::iterator WatchIterator;

	//TODO: replace with better assertions!
	//static_assert(std::is_trivial<AssignInfo>::value, "assignment info not trivial");
	//static_assert(std::is_trivial<Antecedent>::value, "antecedent not trivial");
	//static_assert(std::is_trivial<WatchlistEntry>::value, "watchlist entry not trivial");

	VarConfigStruct() : p_presentCount(0), p_watchSize(0) { }

	void reserveVars(Index count) {
		p_assigns.reserve(count);
		p_varflags.reserve(count);
		p_declevels.reserve(count);
		p_antecedents.reserve(count);

		p_litflags.reserve(2 * count);
		p_occlists.reserve(2 * count);
		p_watchlists.reserve(2 * count);
		p_equivPointer.reserve(2 * count);
	}

	Variable allocVar() {
		Index index = p_assigns.size();
		SYS_ASSERT(SYS_ASRT_GENERAL, p_occlists.size() == 2 * index);
		SYS_ASSERT(SYS_ASRT_GENERAL, p_watchlists.size() == 2 * index);
		
		AssignInfo initial_assign;
		Flags initial_flags = 0;
		Declevel initial_declevel = 0;
		Antecedent initial_antecedent = Antecedent::makeNone();

		Flags initial_litflags1 = 0;
		Flags initial_litflags2 = 0;
		Occlist initial_occlist1;
		Occlist initial_occlist2;
		Watchlist initial_watchlist1;
		Watchlist initial_watchlist2;
		Literal initial_equivalent1 = Literal::fromIndex(2 * index);
		Literal initial_equivalent2 = Literal::fromIndex(2 * index + 1);

		p_assigns.push_back(initial_assign);
		p_varflags.push_back(initial_flags);
		p_declevels.push_back(initial_declevel);
		p_antecedents.push_back(initial_antecedent);

		p_litflags.push_back(initial_litflags1);
		p_litflags.push_back(initial_litflags2);
		p_occlists.emplace_back(std::move(initial_occlist1));
		p_occlists.emplace_back(std::move(initial_occlist2));
		p_watchlists.emplace_back(std::move(initial_watchlist1));
		p_watchlists.emplace_back(std::move(initial_watchlist2));
		p_equivPointer.push_back(initial_equivalent1);
		p_equivPointer.push_back(initial_equivalent2);

		p_presentCount++;
		return Variable::fromIndex(index);
	}
	
	void deleteVar(Variable var) {
		p_varflags[var.getIndex()] |= kVarflagDeleted;
		p_presentCount--;
	}

	Index count() {
		return p_assigns.size();
	}
	Index presentCount() {
		return p_presentCount;
	}
	bool isPresent(Variable var) {
		return !(p_varflags[var.getIndex()] & kVarflagDeleted);
	}
	
	// -----------------------------------------------------------------
	// assignment related functions
	// -----------------------------------------------------------------
	
	void assign(Variable var, bool state) {
		p_assigns[var.getIndex()].assign(state);
	}
	void unassign(Variable var) {
		p_assigns[var.getIndex()].unassign();
	}
	bool isAssigned(Variable var) {
		return p_assigns[var.getIndex()].isAssigned();
	}
	bool isAssignedOne(Variable var) {
		return p_assigns[var.getIndex()].isOne();
	}
	bool isAssignedZero(Variable var) {
		return p_assigns[var.getIndex()].isZero();
	}
	bool litIsTrue(Literal literal) {
		Variable var = literal.variable();
		return p_assigns[var.getIndex()].litIsTrue(literal);
	}
	bool litIsFalse(Literal literal) {
		Variable var = literal.variable();
		return p_assigns[var.getIndex()].litIsFalse(literal);
	}
	
	Declevel getDeclevel(Variable var) {
		return p_declevels[var.getIndex()];
	}
	void setDeclevel(Variable var, Declevel declevel) {
		p_declevels[var.getIndex()] = declevel;
	}
	Antecedent getAntecedent(Variable var) {
		return p_antecedents[var.getIndex()];
	}
	void setAntecedent(Variable var, Antecedent antecedent) {
		p_antecedents[var.getIndex()] = antecedent;
	}

	// -----------------------------------------------------------------
	// flag related functions
	// -----------------------------------------------------------------
	
	bool getVarFlagSaved(Variable var) { return p_varflags[var.getIndex()] & kVarflagSavedOne; }
	void setVarFlagSaved(Variable var) { p_varflags[var.getIndex()] |= kVarflagSavedOne; }
	void clearVarFlagSaved(Variable var) { p_varflags[var.getIndex()] &= ~kVarflagSavedOne; }
	
	Flags getVarFlagModel(Variable var) { return p_varflags[var.getIndex()] & kVarflagModelOne; }
	void setVarFlagModel(Variable var) { p_varflags[var.getIndex()] |= kVarflagModelOne; }
	void clearVarFlagModel(Variable var) { p_varflags[var.getIndex()] &= ~kVarflagModelOne; }
	
	Flags getVarFlagProtected(Variable var) { return p_varflags[var.getIndex()] & kVarflagProtected; }
	void setVarFlagProtected(Variable var) { p_varflags[var.getIndex()] |= kVarflagProtected; }
	void clearVarFlagProtected(Variable var) { p_varflags[var.getIndex()] &= ~kVarflagProtected; }

	bool getLitFlagMarked(Literal lit) { return p_litflags[lit.getIndex()] & kLitflagMarked; }
	void setLitFlagMarked(Literal lit) { p_litflags[lit.getIndex()] |= kLitflagMarked; }
	void clearLitFlagMarked(Literal lit) { p_litflags[lit.getIndex()] &= ~kLitflagMarked; }
	
	bool getLitFlagAssumption(Literal lit) { return p_litflags[lit.getIndex()] & kLitflagAssumption; }
	void setLitFlagAssumption(Literal lit) { p_litflags[lit.getIndex()] |= kLitflagAssumption; }
	void clearLitFlagAssumption(Literal lit) { p_litflags[lit.getIndex()] &= ~kLitflagAssumption; }

	// -----------------------------------------------------------------
	// occlist related functions
	// -----------------------------------------------------------------
	
	void occurInsert(Literal literal, Clause clause);
	void occurRemove(Literal literal, Clause clause);
	void occurClear(Literal literal) {
		p_occlists[literal.getIndex()].clear();
	}
	void occurShrink(Literal literal) {
		p_occlists[literal.getIndex()].shrink();
	}
	void occurErase(Literal literal, OccurIterator iterator) {
		p_occlists[literal.getIndex()].erase(iterator);
	}
	void occurCut(Literal literal, OccurIterator iterator) {
		p_occlists[literal.getIndex()].cutoff(iterator);
	}
	unsigned int occurSize(Literal literal) {
		return p_occlists[literal.getIndex()].size();
	}
	OccurIterator occurBegin(Literal literal) {
		return p_occlists[literal.getIndex()].begin();
	}
	OccurIterator occurEnd(Literal literal) {
		return p_occlists[literal.getIndex()].end();
	}

	// -----------------------------------------------------------------
	// watchlist related functions
	// -----------------------------------------------------------------

	unsigned int watchOverallSize() {
		return p_watchSize;
	}
	void watchInsert(Literal literal, WatchlistEntry entry) {
		Watchlist &watchlist = p_watchlists[literal.getIndex()];
		watchlist.insert(entry);
		p_watchSize++;
	}
	unsigned int watchSize(Literal literal) {
		Watchlist &watch_list = p_watchlists[literal.getIndex()];
		return watch_list.size();
	}
	WatchIterator watchBegin(Literal literal) {
		Watchlist &watch_list = p_watchlists[literal.getIndex()];
		return watch_list.begin();
	}
	WatchIterator watchEnd(Literal literal) {
		Watchlist &watch_list = p_watchlists[literal.getIndex()];
		return watch_list.end();
	}
	void watchCut(Literal literal, WatchIterator it) {
		Watchlist &watch_list = p_watchlists[literal.getIndex()];
		p_watchSize -= watch_list.end() - it;
		watch_list.cutoff(it);
	}
	void watchErase(Literal literal, WatchIterator it) {
		Watchlist &watch_list = p_watchlists[literal.getIndex()];
		watch_list.erase(it);
		p_watchSize--;
	}
	void watchClear(Literal literal) {
		Watchlist &watch_list = p_watchlists[literal.getIndex()];
		p_watchSize -= watch_list.size();
		watch_list.clear();
	}

	// -----------------------------------------------------------------
	// equivalence related functions
	// -----------------------------------------------------------------

	Literal equivalent(Literal literal) {
		Literal root = literal;
		while(p_equivPointer[root.getIndex()] != root)
			root = p_equivPointer[root.getIndex()];
		// path compression
		Literal current = literal;
		while(p_equivPointer[current.getIndex()] != root) {
			Literal parent = p_equivPointer[current.getIndex()];
			p_equivPointer[current.getIndex()] = root;
			current = parent;
		}
		return root;
	}
	void join(Literal literal1, Literal literal2) {
		// NOTE: no union-by-size for now: the literal with smaller index becomes root
		// this makes sure the root any literal and its inverse are always inverse
		Literal root1 = equivalent(literal1);
		Literal root2 = equivalent(literal2);
		if(root1.getIndex() < root2.getIndex()) {
			p_equivPointer[root2.getIndex()] = root1;
		}else p_equivPointer[root1.getIndex()] = root2;
	}

private:
	// per variable configuration
	std::vector<AssignInfo> p_assigns;
	std::vector<Flags> p_varflags;
	std::vector<Declevel> p_declevels;
	std::vector<Antecedent> p_antecedents;
	
	// per literal configuration
	std::vector<Flags> p_litflags;
	std::vector<Occlist> p_occlists;
	std::vector<Watchlist> p_watchlists;
	std::vector<Literal> p_equivPointer;
	
	Index p_presentCount;
	unsigned int p_watchSize;
};

template<typename BaseDefs>
class VarIterator
	: std::iterator<std::forward_iterator_tag, VariableType<BaseDefs>> {
public:
	VarIterator(typename BaseDefs::LiteralIndex index) : p_index(index) {
	}

	void operator++ () {
		p_index++;
	}
	bool operator== (const VarIterator<BaseDefs> &other) {
		return p_index == other.p_index;
	}
	bool operator!= (const VarIterator<BaseDefs> &other) {
		return !(*this == other);
	}

	VariableType<BaseDefs> operator* () {
		return VariableType<BaseDefs>::fromIndex(p_index);
	}

private:
	typename BaseDefs::LiteralIndex p_index;
};

}; // namespace satuzk

