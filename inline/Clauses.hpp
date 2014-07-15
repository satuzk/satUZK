
namespace satuzk {

template<typename BaseDefs>
class ClauseType {
public:
	typedef typename BaseDefs::ClauseIndex Index;

	static ClauseType<BaseDefs> illegalClause() {
		return ClauseType(-1);
	}
	static ClauseType<BaseDefs> fromIndex(Index index) {
		return ClauseType(index);
	}
	
	ClauseType() : p_index(-1) { }

	Index getIndex() {
		return p_index;
	}

	bool operator== (const ClauseType<BaseDefs> &other) {
		return p_index == other.p_index;
	}
	bool operator!= (const ClauseType<BaseDefs> &other) {
		return p_index != other.p_index;
	}

private:
	explicit ClauseType(Index index) : p_index(index) { }
	
	Index p_index;
};

template<typename BaseDefs>
class ClauseRef;

template<typename BaseDefs>
class ClauseIteratorStruct;

template<typename BaseDefs>
class ClauseSpaceStruct {
friend class ClauseRef<BaseDefs>;
public:
	static const int kClauseAlign = 8;
	
	typedef LiteralType<BaseDefs> Literal;
	typedef typename BaseDefs::ClauseIndex Index;
	typedef ClauseType<BaseDefs> Clause;
	typedef typename BaseDefs::ClauseLitIndex LitIndex;
	typedef typename BaseDefs::Activity Activity;
	
	struct ClauseHead {
		// clause is installed i.e. in watch lists
		static const uint32_t kFlagInstalled = 1;
		// marked for deletion
		static const uint32_t kFlagDelete = 2;
		// marked essential i.e. must not be deleted
		static const uint32_t kFlagEssential = 4;
		// clause is frozen, i.e. not installed but will be reinstalled later
		static const uint32_t kFlagFrozen = 8;
		// the clause has been marked by some algorithm.
		// algorithms should remove this mark after they are done.
		static const uint32_t kFlagMarked = 16;
		// clause has been moved/dropped during garbage collection
		static const uint32_t kFlagCollectMoved = 32;
		static const uint32_t kFlagCollectDropped = 64;
		static const uint32_t kFlagImproved = 128;
		static const uint32_t kFlagCheckedDist = 256;
		static const uint32_t kFlagCreatedVecd = 512;
		static const uint32_t kFlagCheckedSsub = 1024;

		// flags defined above
		uint32_t flags;
		// number of literals of the clause
		LitIndex numLiterals;

		/* we need a default constructor so that this structure
			remains standard-layout */
		ClauseHead() = default;
		ClauseHead(LitIndex num_literals) : flags(0),
				numLiterals(num_literals) { }

		// allows access to the clause body
		void *access(int offset) {
			return (void*)((uintptr_t)this + sizeof(ClauseHead) + offset);
		}
		// allows access to literals
		Literal *literal(LitIndex index) {
			Literal *litptr = (Literal*)access(index * sizeof(Literal));
			return litptr;
		}
	};

	struct ClauseTail {
		union {
			struct {
				Activity activity;
				uint64_t signature;
				uint16_t lbd;
			} normalTail;
			
			struct {
				Index destination;
			} movedTail;
		};
	};
	
	static_assert(std::is_pod<ClauseHead>::value, "head not trivial");
	static_assert(std::is_pod<ClauseTail>::value, "tail not trivial");

	typedef typename std::vector<Index>::iterator IndexIterator;
	typedef ClauseIteratorStruct<BaseDefs> ClauseIterator;

	ClauseSpaceStruct() : p_allocator(kClauseAlign),
			p_presentClauses(0), p_presentBytes(0), p_presentLiterals(0),
			p_deletedClauses(0) { }
	ClauseSpaceStruct(const ClauseSpaceStruct &other) = delete;
	ClauseSpaceStruct &operator= (const ClauseSpaceStruct &other) = delete;

	ClauseSpaceStruct &operator= (ClauseSpaceStruct &&other) {
		p_allocator = std::move(other.p_allocator);
		p_indices = std::move(other.p_indices);
		p_presentBytes = other.p_presentBytes;
		p_presentClauses = other.p_presentClauses;
		p_deletedClauses = other.p_deletedClauses;
		p_presentLiterals = other.p_presentLiterals;
		return *this;
	}

//FIXME:private:
	LitIndex p_padLength(LitIndex length) {
		if(length % 2 == 1)
			return length + 1;
		return length;
	}
	ClauseHead *p_accessHead(Index index) {
		return (ClauseHead*)p_allocator[index];
	}
	ClauseTail *p_accessTail(Index index) {
		auto headptr = p_accessHead(index);
		auto adj_length = p_padLength(headptr->numLiterals);
		return (ClauseTail*)headptr->access(adj_length * sizeof(Literal));
	}

public:
	Index calcBytes(LitIndex length) {
		return sizeof(ClauseHead)
				+ p_padLength(length) * sizeof(Literal)
				+ sizeof(ClauseTail);
	}

	Clause allocClause(LitIndex length) {
		// allocate memory for the clause
		Index bytelen = calcBytes(length);
		Index index = p_allocator.alloc(bytelen);

		// we have to setup the head before we can use p_accessTail()!
		ClauseHead *head = p_accessHead(index);
		new (head) ClauseHead(length);

		auto tail = p_accessTail(index);
		tail->normalTail.lbd = 0;
		tail->normalTail.activity = 0.0f;
		tail->normalTail.signature = 0;

		p_indices.push_back(index);
		p_presentBytes += bytelen;
		p_presentClauses++;
		p_presentLiterals += length;
		return Clause::fromIndex(index);
	}

	ClauseIterator begin();
	ClauseIterator end();

	unsigned int numClauses() {
		return p_indices.size();
	}
	unsigned int numPresent() {
		return p_presentClauses;
	}
	uint64_t presentBytes() {
		return p_presentBytes;
	}
	uint64_t presentLiterals() {
		return p_presentLiterals;
	}
	unsigned int numDeleted() {
		return p_deletedClauses;
	}
	
	LitIndex clauseLength(Clause clause) {
		return p_accessHead(clause.getIndex())->numLiterals;
	}
	void clauseSetLiteral(Clause clause, LitIndex index, Literal literal) {
		*p_accessHead(clause.getIndex())->literal(index) = literal;
	}
	Literal clauseGetLiteral(Clause clause, LitIndex index) {
		return *p_accessHead(clause.getIndex())->literal(index);
	}

	void clauseSetActivity(Clause clause, Activity activity) {
		p_accessTail(clause.getIndex())->normalTail.activity = activity;
	}
	Activity clauseGetActivity(Clause clause) {
		return p_accessTail(clause.getIndex())->normalTail.activity;
	}

	void clauseSetSignature(Clause clause, uint64_t signature) {
		p_accessTail(clause.getIndex())->normalTail.signature = signature;
	}
	uint64_t clauseGetSignature(Clause clause) {
		return p_accessTail(clause.getIndex())->normalTail.signature;
	}

	void clauseSetLbd(Clause clause, uint16_t lbd) {
		p_accessTail(clause.getIndex())->normalTail.lbd = lbd;
	}
	uint16_t clauseGetLbd(Clause clause) {
		return p_accessTail(clause.getIndex())->normalTail.lbd;
	}

	void deleteClause(Clause clause) {
		p_accessHead(clause.getIndex())->flags |= ClauseHead::kFlagDelete;
		p_presentClauses--;
		p_deletedClauses++;
		p_presentBytes -= calcBytes(p_accessHead(clause.getIndex())->numLiterals);
		p_presentLiterals += p_accessHead(clause.getIndex())->numLiterals;
	}
	bool isDeleted(Clause clause) {
		return p_accessHead(clause.getIndex())->flags & ClauseHead::kFlagDelete;
	}

	void setFlagEssential(Clause clause) { p_accessHead(clause.getIndex())->flags |= ClauseHead::kFlagEssential; }
	bool getFlagEssential(Clause clause) { return p_accessHead(clause.getIndex())->flags & ClauseHead::kFlagEssential; }
	void unsetFlagEssential(Clause clause) { p_accessHead(clause.getIndex())->flags &= ~ClauseHead::kFlagEssential; }

	void setFlagInstalled(Clause clause) { p_accessHead(clause.getIndex())->flags |= ClauseHead::kFlagInstalled; }
	bool getFlagInstalled(Clause clause) { return p_accessHead(clause.getIndex())->flags & ClauseHead::kFlagInstalled; }
	void unsetFlagInstalled(Clause clause) { p_accessHead(clause.getIndex())->flags &= ~ClauseHead::kFlagInstalled; }

	void setFlagFrozen(Clause clause) { p_accessHead(clause.getIndex())->flags |= ClauseHead::kFlagFrozen; }
	bool getFlagFrozen(Clause clause) { return p_accessHead(clause.getIndex())->flags & ClauseHead::kFlagFrozen; }
	void unsetFlagFrozen(Clause clause) { p_accessHead(clause.getIndex())->flags &= ~ClauseHead::kFlagFrozen; }
	
	void setFlagMarked(Clause clause) { p_accessHead(clause.getIndex())->flags |= ClauseHead::kFlagMarked; }
	bool getFlagMarked(Clause clause) { return p_accessHead(clause.getIndex())->flags & ClauseHead::kFlagMarked; }
	void unsetFlagMarked(Clause clause) { p_accessHead(clause.getIndex())->flags &= ~ClauseHead::kFlagMarked; }

	bool getFlagImproved(Clause clause) { return p_accessHead(clause.getIndex())->flags & ClauseHead::kFlagImproved; }
	void setFlagImproved(Clause clause) { p_accessHead(clause.getIndex())->flags |= ClauseHead::kFlagImproved; }
	void unsetFlagImproved(Clause clause) { p_accessHead(clause.getIndex())->flags &= ~ClauseHead::kFlagImproved; }
	
	void setFlagCheckedDist(Clause clause) { p_accessHead(clause.getIndex())->flags |= ClauseHead::kFlagCheckedDist; }
	bool getFlagCheckedDist(Clause clause) { return p_accessHead(clause.getIndex())->flags & ClauseHead::kFlagCheckedDist; }

	void setFlagCreatedVecd(Clause clause) { p_accessHead(clause.getIndex())->flags |= ClauseHead::kFlagCreatedVecd; }

	void setFlagCheckedSsub(Clause clause) { p_accessHead(clause.getIndex())->flags |= ClauseHead::kFlagCheckedSsub; }
	bool getFlagCheckedSsub(Clause clause) { return p_accessHead(clause.getIndex())->flags & ClauseHead::kFlagCheckedSsub; }

	std::vector<Index> p_indices;
	util::memory::BulkAllocator<typename BaseDefs::ClauseIndex> p_allocator;

	unsigned int p_presentClauses;
	uint64_t p_presentBytes;
	uint64_t p_presentLiterals;
	unsigned int p_deletedClauses;
};

template<typename BaseDefs>
class ClauseRef {
private:
	ClauseRef(ClauseSpaceStruct<BaseDefs> &config, typename BaseDefs::ClauseIndex clause_id)
			: p_config(config), p_clauseId(clause_id) {
	}
	
	bool wasMoved();
	bool wasDropped();
	typename BaseDefs::ClauseIndex getMoveDestination();
	
public:
	ClauseSpaceStruct<BaseDefs> &p_config;
	typename BaseDefs::ClauseIndex p_clauseId;
};

template<typename BaseDefs>
bool ClauseRef<BaseDefs>::wasMoved() {
	return p_config.p_accessHead(p_clauseId)->flags & ClauseSpaceStruct<BaseDefs>::ClauseHead::kFlagCollectMoved;
}
template<typename BaseDefs>
bool ClauseRef<BaseDefs>::wasDropped() {
	return p_config.p_accessHead(p_clauseId)->flags & ClauseSpaceStruct<BaseDefs>::ClauseHead::kFlagCollectDropped;
}
template<typename BaseDefs>
typename BaseDefs::ClauseIndex ClauseRef<BaseDefs>::getMoveDestination() {
	assert(wasMoved());
	return p_config.p_accessTail(p_clauseId)->movedTail.destination;
}

template<typename BaseDefs, typename Callback>
void relocateClauses(ClauseSpaceStruct<BaseDefs> &from_config, ClauseSpaceStruct<BaseDefs> &to_config, Callback &callback) {
	for(typename ClauseSpaceStruct<BaseDefs>::IndexIterator it = from_config.p_indices.begin();
			it != from_config.p_indices.end(); ++it) {
		typename ClauseSpaceStruct<BaseDefs>::Index from_index = *it;
		typename ClauseSpaceStruct<BaseDefs>::ClauseHead *from_head = from_config.p_accessHead(from_index);
		if(from_head->flags & ClauseSpaceStruct<BaseDefs>::ClauseHead::kFlagDelete) {
			callback.onErase(ClauseType<BaseDefs>::fromIndex(from_index));
			from_head->flags |= ClauseSpaceStruct<BaseDefs>::ClauseHead::kFlagCollectDropped;
		}else{
			// allocate the new clause
			typename ClauseSpaceStruct<BaseDefs>::Clause to_clause = to_config.allocClause(from_head->numLiterals);
			typename ClauseSpaceStruct<BaseDefs>::Index to_index = to_clause.getIndex();
			auto from_tail = from_config.p_accessTail(from_index);
			typename ClauseSpaceStruct<BaseDefs>::ClauseHead *to_head = to_config.p_accessHead(to_index);
			auto to_tail = to_config.p_accessTail(to_index);
			
			// copy all flags, literals, etc.
			for(typename ClauseSpaceStruct<BaseDefs>::LitIndex i = 0; i < from_head->numLiterals; i++) {
				typename ClauseSpaceStruct<BaseDefs>::Literal literal = *from_head->literal(i);
				*to_head->literal(i) = literal;
			}
			to_head->flags = from_head->flags;
			to_tail->normalTail.lbd = from_tail->normalTail.lbd;
			to_tail->normalTail.activity = from_tail->normalTail.activity;
			to_tail->normalTail.signature = from_tail->normalTail.signature;

			callback.onMove(ClauseType<BaseDefs>::fromIndex(from_index), to_clause);
			from_head->flags |= ClauseSpaceStruct<BaseDefs>::ClauseHead::kFlagCollectMoved;
			from_tail->movedTail.destination = to_index;
		}
	}
	from_config.p_deletedClauses = 0;
}

template<typename BaseDefs>
class ClauseIteratorStruct
	: public std::iterator<std::forward_iterator_tag, ClauseType<BaseDefs>> {
public:
	ClauseIteratorStruct(ClauseSpaceStruct<BaseDefs> &config,
			typename ClauseSpaceStruct<BaseDefs>::IndexIterator iterator)
		: p_iterator(iterator) { }

	void operator++ () {
		p_iterator++;
	}
	bool operator== (const ClauseIteratorStruct<BaseDefs> &other) const {
		return p_iterator == other.p_iterator;
	}
	bool operator!= (const ClauseIteratorStruct<BaseDefs> &other) const {
		return p_iterator != other.p_iterator;
	}
	ClauseType<BaseDefs> operator* () {
		return ClauseType<BaseDefs>::fromIndex(*p_iterator);
	}
	
private:
	typename ClauseSpaceStruct<BaseDefs>::IndexIterator p_iterator;
};

template<typename BaseDefs>
class ClauseLitIteratorStruct
	: public std::iterator<std::forward_iterator_tag, LiteralType<BaseDefs>> {
public:
	ClauseLitIteratorStruct(ClauseSpaceStruct<BaseDefs> &config, ClauseType<BaseDefs> clause,
			typename BaseDefs::ClauseLitIndex index) {
		p_pointer = config.p_accessHead(clause.getIndex())->literal(index);
	}

	void operator++ () {
		p_pointer++;
	}
	bool operator== (const ClauseLitIteratorStruct<BaseDefs> &other) const {
		return p_pointer == other.p_pointer;
	}
	bool operator!= (const ClauseLitIteratorStruct<BaseDefs> &other) const {
		return p_pointer != other.p_pointer;
	}
	LiteralType<BaseDefs> &operator* () {
		return *p_pointer;
	}
	
private:
	LiteralType<BaseDefs> *p_pointer;
};

template<typename BaseDefs>
typename ClauseSpaceStruct<BaseDefs>::ClauseIterator ClauseSpaceStruct<BaseDefs>::begin() {
	return ClauseIterator(*this, p_indices.begin());
}
template<typename BaseDefs>
typename ClauseSpaceStruct<BaseDefs>::ClauseIterator ClauseSpaceStruct<BaseDefs>::end() {
	return ClauseIterator(*this, p_indices.end());
}

}; // namespace satuzk

