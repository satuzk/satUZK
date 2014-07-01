
namespace satuzk {

template<typename BaseDefs>
class LiteralType;

template<typename BaseDefs>
class ClauseType;

template<typename BaseDefs>
class AntecedentStruct {
public:
	typedef LiteralType<BaseDefs> Literal;
	typedef ClauseType<BaseDefs> Clause;

	static AntecedentStruct<BaseDefs> makeNone();
	static AntecedentStruct<BaseDefs> makeDecision();
	static AntecedentStruct<BaseDefs> makeClause(Clause clause);
	static AntecedentStruct<BaseDefs> makeBinary(Literal literal);

	bool operator== (AntecedentStruct<BaseDefs> other);
	bool operator!= (AntecedentStruct<BaseDefs> other);

	bool isDecision();
	bool isClause();
	bool isBinary();

	Clause getClause();
	Literal getBinary();

private:
	static const uint32_t kTypeNone = 0;
	static const uint32_t kTypeDecision = 1;
	static const uint32_t kTypeClause = 2;
	static const uint32_t kTypeBinary = 3;
	
	uint32_t type;

	union {
		uint32_t padding;
		typename BaseDefs::ClauseIndex clause;
		typename BaseDefs::LiteralIndex literal;
	} identifier;
};

template<typename Config>
class AntecedentIteratorStruct {
public:
	AntecedentIteratorStruct(Config &config,
			typename Config::Antecedent antecedent,
			typename Config::ClauseLitIndex index)
			: p_config(config), p_antecedent(antecedent), p_index(index) { }

	static AntecedentIteratorStruct begin(Config &config,
			typename Config::Antecedent antecedent) {
		return AntecedentIteratorStruct(config, antecedent, 0);
	}
	static AntecedentIteratorStruct end(Config &config,
			typename Config::Antecedent antecedent) {
		if(antecedent.isBinary()) {
			return AntecedentIteratorStruct(config, antecedent, 1);
		}else if(antecedent.isClause()) {
			typename Config::Clause clause = antecedent.getClause();
			return AntecedentIteratorStruct(config, antecedent,
					config.clauseLength(clause) - 1);
		}else if(antecedent.isDecision()) {
			SYS_CRITICAL("Decision antecedent\n");
		}else SYS_CRITICAL("Illegal antecedent\n");
	}

	void operator++ () {
		++p_index;
	}
	bool operator== (const AntecedentIteratorStruct<Config> &other) const {
		return p_index == other.p_index;
	}
	bool operator!= (const AntecedentIteratorStruct<Config> &other) const {
		return p_index != other.p_index;
	}
	typename Config::Literal operator* () {
		if(p_antecedent.isBinary()) {
			SYS_ASSERT(SYS_ASRT_GENERAL, p_index == 0);
			return p_antecedent.getBinary();
		}else if(p_antecedent.isClause()) {
			typename Config::Clause clause = p_antecedent.getClause();
			return p_config.clauseGetLiteral(clause, p_index + 1).inverse();
		}else SYS_CRITICAL("Illegal antecedent\n");
	}
	
private:
	Config &p_config;
	typename Config::Antecedent p_antecedent;
	typename Config::ClauseLitIndex p_index;
};

}; // namespace satuzk

