
namespace satuzk {

template<typename BaseDefs>
class LiteralType;

template<typename BaseDefs>
class ClauseType;

template<typename BaseDefs>
class ConflictStruct {
public:
	typedef LiteralType<BaseDefs> Literal;
	typedef ClauseType<BaseDefs> Clause;
	
	static ConflictStruct<BaseDefs> makeNone();
	static ConflictStruct<BaseDefs> makeClause(Clause clause);
	static ConflictStruct<BaseDefs> makeBinary(Literal literal1, Literal literal2);
	static ConflictStruct<BaseDefs> makeFact(Literal literal);
	static ConflictStruct<BaseDefs> makeEmpty();

	bool operator== (AntecedentStruct<BaseDefs> other);
	bool operator!= (AntecedentStruct<BaseDefs> other);

	bool isNone();
	bool isClause();
	bool isBinary();
	bool isFact();
	bool isEmpty();

	Clause getClause();
	Literal getLiteral1();
	Literal getLiteral2();

private:
	static const uint32_t kTypeNone = 0;
	static const uint32_t kTypeClause = 1;
	static const uint32_t kTypeBinary = 2;
	static const uint32_t kTypeFact = 3;
	static const uint32_t kTypeEmpty = 4;

	uint32_t type;
	
	union {
		struct {
			uint32_t p1;
			uint32_t p2;
		} padding;
		typename BaseDefs::ClauseIndex clause;
		struct {
			typename BaseDefs::LiteralIndex l1;
			typename BaseDefs::LiteralIndex l2;
		} literals;
	};
};

template<typename Config>
class ConflictIteratorStruct {
public:
	ConflictIteratorStruct(Config &config,
			typename Config::Conflict conflict,
			typename Config::ClauseLitIndex index)
			: p_config(config), p_conflict(conflict), p_index(index) { }

	static ConflictIteratorStruct begin(Config &config,
			typename Config::Conflict conflict) {
		return ConflictIteratorStruct(config, conflict, 0);
	}
	static ConflictIteratorStruct end(Config &config,
			typename Config::Conflict conflict) {
		if(conflict.isFact()) {
			return ConflictIteratorStruct(config, conflict, 1);
		}else if(conflict.isBinary()) {
			return ConflictIteratorStruct(config, conflict, 2);
		}else if(conflict.isClause()) {
			typename Config::Clause clause = conflict.getClause();
			return ConflictIteratorStruct(config, conflict,
					config.clauseLength(clause));
		}else SYS_CRITICAL("Illegal conflict\n");
	}

	void operator++ () {
		++p_index;
	}
	bool operator== (const ConflictIteratorStruct<Config> &other) const {
		return p_index == other.p_index;
	}
	bool operator!= (const ConflictIteratorStruct<Config> &other) const {
		return p_index != other.p_index;
	}
	typename Config::Literal operator* () {
		if(p_conflict.isFact()) {
			SYS_ASSERT(SYS_ASRT_GENERAL, p_index == 0);
			return p_conflict.getLiteral1();
		}else if(p_conflict.isBinary() && p_index == 0) {
			return p_conflict.getLiteral1();
		}else if(p_conflict.isBinary() && p_index == 1) {
			return p_conflict.getLiteral2();
		}else if(p_conflict.isClause()) {
			typename Config::Clause clause = p_conflict.getClause();
			return p_config.clauseGetLiteral(clause, p_index).inverse();
		}else SYS_CRITICAL("Illegal conflict\n");
	}
	
private:
	Config &p_config;
	typename Config::Conflict p_conflict;
	typename Config::ClauseLitIndex p_index;
};

}; // namespace satuzk

