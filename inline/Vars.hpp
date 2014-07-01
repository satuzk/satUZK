
namespace satuzk {

// -----------------------------------------------------------------
// WatchlistEntryStruct class
// -----------------------------------------------------------------

template<typename BaseDefs>
void WatchlistEntryStruct<BaseDefs>::setLong() {
	field1 &= ~kFlagMask;
}
template<typename BaseDefs>
void WatchlistEntryStruct<BaseDefs>::setBinary() {
	field1 &= ~kFlagMask;
	field1 |= kFlagBinary;
}
template<typename BaseDefs>
bool WatchlistEntryStruct<BaseDefs>::isLong() {
	return (field1 & kFlagMask) == kFlagLong;
}
template<typename BaseDefs>
bool WatchlistEntryStruct<BaseDefs>::isBinary() {
	return (field1 & kFlagMask) == kFlagBinary;
}

template<typename BaseDefs>
void WatchlistEntryStruct<BaseDefs>::binarySetImplied(Literal literal) {
	field2 = literal.getIndex();
}
template<typename BaseDefs>
void WatchlistEntryStruct<BaseDefs>::binarySetClause(Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, !(clause.getIndex() & kFlagMask));
	field1 = clause.getIndex() | kFlagBinary;
}
template<typename BaseDefs>
typename WatchlistEntryStruct<BaseDefs>::Literal WatchlistEntryStruct<BaseDefs>::binaryGetImplied() {
	return Literal::fromIndex(field2);
}
template<typename BaseDefs>
typename WatchlistEntryStruct<BaseDefs>::Clause WatchlistEntryStruct<BaseDefs>::binaryGetClause() {
	return Clause::fromIndex(field1 & ~kFlagMask);
}

template<typename BaseDefs>
void WatchlistEntryStruct<BaseDefs>::longSetClause(Clause clause) {
	SYS_ASSERT(SYS_ASRT_GENERAL, !(clause.getIndex() & kFlagMask));
	field1 = clause.getIndex();
}
template<typename BaseDefs>
void WatchlistEntryStruct<BaseDefs>::longSetBlocking(Literal literal) {
	field2 = literal.getIndex();
}
template<typename BaseDefs>
typename WatchlistEntryStruct<BaseDefs>::Clause WatchlistEntryStruct<BaseDefs>::longGetClause() {
	return Clause::fromIndex(field1);
}
template<typename BaseDefs>
typename WatchlistEntryStruct<BaseDefs>::Literal WatchlistEntryStruct<BaseDefs>::longGetBlocking() {
	return Literal::fromIndex(field2);
}

// -----------------------------------------------------------------
// VarConfigStruct class
// -----------------------------------------------------------------

template<typename BaseDefs>
void VarConfigStruct<BaseDefs>::occurInsert(Literal literal, Clause clause) {
	p_occlists[literal.getIndex()].insert(clause);
}
template<typename BaseDefs>
void VarConfigStruct<BaseDefs>::occurRemove(Literal literal, Clause clause) {
	auto it = std::find(p_occlists[literal.getIndex()].begin(),
			p_occlists[literal.getIndex()].end(), clause);
	SYS_ASSERT(SYS_ASRT_GENERAL, it != p_occlists[literal.getIndex()].end());
	p_occlists[literal.getIndex()].erase(it);
}

}; // namespace satuzk

