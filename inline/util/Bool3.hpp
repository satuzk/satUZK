
class Bool3 {
friend Bool3 false3();
friend Bool3 true3();
friend Bool3 undefined3();
public:
	bool operator== (Bool3 other) {
		return ((other.p_value & 2) & (p_value & 2))
			| (!(other.p_value & 2) & (p_value == other.p_value));
	}
	bool operator!= (Bool3 other) {
		return !(operator== (other));
	}
	Bool3 operator^ (bool other) {
		return Bool3(p_value ^ (uint8_t)other);
	}
	
	bool isTrue() {
		return !(p_value & 2) && (p_value & 1);
	}
	bool isFalse() {
		return !(p_value & 2) && !(p_value & 1);
	}
	bool isUndefined() {
		return p_value & 2;
	}
	
private:
	explicit Bool3(int value) : p_value(value) { }
	uint8_t p_value;
};

inline Bool3 false3() { return Bool3(0); }
inline Bool3 true3() { return Bool3(1); }
inline Bool3 undefined3() { return Bool3(2); }


