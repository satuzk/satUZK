
namespace satuzk {

template<typename BaseDefs>
ConflictStruct<BaseDefs> ConflictStruct<BaseDefs>::makeNone() {
	ConflictStruct<BaseDefs> result;
	result.type = kTypeNone;
	result.padding.p1 = 0;
	result.padding.p2 = 0;
	return result;
}
template<typename BaseDefs>
ConflictStruct<BaseDefs> ConflictStruct<BaseDefs>::makeClause(Clause clause) {
	ConflictStruct<BaseDefs> result;
	result.padding.p1 = 0;
	result.padding.p2 = 0;
	result.type = kTypeClause;
	result.clause = clause.getIndex();
	return result;
}
template<typename BaseDefs>
ConflictStruct<BaseDefs> ConflictStruct<BaseDefs>::makeBinary(Literal literal1, Literal literal2) {
	ConflictStruct<BaseDefs> result;
	result.padding.p1 = 0;
	result.padding.p2 = 0;
	result.type = kTypeBinary;
	result.literals.l1 = literal1.getIndex();
	result.literals.l2 = literal2.getIndex();
	return result;
}
template<typename BaseDefs>
ConflictStruct<BaseDefs> ConflictStruct<BaseDefs>::makeFact(Literal literal) {
	ConflictStruct<BaseDefs> result;
	result.padding.p1 = 0;
	result.padding.p2 = 0;
	result.type = kTypeFact;
	result.literals.l1 = literal.getIndex();
	return result;
}
template<typename BaseDefs>
ConflictStruct<BaseDefs> ConflictStruct<BaseDefs>::makeEmpty() {
	ConflictStruct<BaseDefs> result;
	result.padding.p1 = 0;
	result.padding.p2 = 0;
	result.type = kTypeEmpty;
	return result;
}

template<typename BaseDefs>
bool ConflictStruct<BaseDefs>::operator== (AntecedentStruct<BaseDefs> other) {
	if(type != other.type)
		return false;
	if(padding != other.padding)
		return false;
	return true;
}
template<typename BaseDefs>
bool ConflictStruct<BaseDefs>::operator!= (AntecedentStruct<BaseDefs> other) {
	return !(*this == other);
}

template<typename BaseDefs>
bool ConflictStruct<BaseDefs>::isNone() {
	return type == kTypeNone;
}
template<typename BaseDefs>
bool ConflictStruct<BaseDefs>::isClause() {
	return type == kTypeClause;
}
template<typename BaseDefs>
bool ConflictStruct<BaseDefs>::isBinary() {
	return type == kTypeBinary;
}
template<typename BaseDefs>
bool ConflictStruct<BaseDefs>::isFact() {
	return type == kTypeFact;
}
template<typename BaseDefs>
bool ConflictStruct<BaseDefs>::isEmpty() {
	return type == kTypeEmpty;
}

template<typename BaseDefs>
typename ConflictStruct<BaseDefs>::Clause ConflictStruct<BaseDefs>::getClause() {
	return Clause::fromIndex(clause);
}
template<typename BaseDefs>
typename ConflictStruct<BaseDefs>::Literal ConflictStruct<BaseDefs>::getLiteral1() {
	return Literal::fromIndex(literals.l1);
}
template<typename BaseDefs>
typename ConflictStruct<BaseDefs>::Literal ConflictStruct<BaseDefs>::getLiteral2() {
	return Literal::fromIndex(literals.l2);
}

}; // namespace satuzk

