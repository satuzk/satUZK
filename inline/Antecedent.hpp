
namespace satuzk {

template<typename BaseDefs>
AntecedentStruct<BaseDefs> AntecedentStruct<BaseDefs>::makeNone() {
	AntecedentStruct<BaseDefs> antecedent;
	antecedent.type = kTypeNone;
	antecedent.identifier.padding = 0;
	return antecedent;
}
template<typename BaseDefs>
AntecedentStruct<BaseDefs> AntecedentStruct<BaseDefs>::makeDecision() {
	AntecedentStruct<BaseDefs> antecedent;
	antecedent.type = kTypeDecision;
	antecedent.identifier.padding = 0;
	return antecedent;
}
template<typename BaseDefs>
AntecedentStruct<BaseDefs> AntecedentStruct<BaseDefs>::makeClause(Clause clause) {
	AntecedentStruct<BaseDefs> antecedent;
	antecedent.type = kTypeClause;
	antecedent.identifier.clause = clause.getIndex();
	return antecedent;
}
template<typename BaseDefs>
AntecedentStruct<BaseDefs> AntecedentStruct<BaseDefs>::makeBinary(Literal literal) {
	AntecedentStruct<BaseDefs> antecedent;
	antecedent.type = kTypeBinary;
	antecedent.identifier.literal = literal.getIndex();
	return antecedent;
}

template<typename BaseDefs>
bool AntecedentStruct<BaseDefs>::operator== (AntecedentStruct<BaseDefs> other) {
	if(type != other.type)
		return false;
	if(identifier.padding != other.identifier.padding)
		return false;
	return true;
}
template<typename BaseDefs>
bool AntecedentStruct<BaseDefs>::operator!= (AntecedentStruct<BaseDefs> other) {
	return !(*this == other);
}

template<typename BaseDefs>
bool AntecedentStruct<BaseDefs>::isDecision() {
	return type == kTypeDecision;
}
template<typename BaseDefs>
bool AntecedentStruct<BaseDefs>::isClause() {
	return type == kTypeClause;
}
template<typename BaseDefs>
bool AntecedentStruct<BaseDefs>::isBinary() {
	return type == kTypeBinary;
}

template<typename BaseDefs>
typename AntecedentStruct<BaseDefs>::Clause AntecedentStruct<BaseDefs>::getClause() {
	return Clause::fromIndex(identifier.clause);
}
template<typename BaseDefs>
typename AntecedentStruct<BaseDefs>::Literal AntecedentStruct<BaseDefs>::getBinary() {
	return Literal::fromIndex(identifier.literal);
}

}; // namespace satuzk

