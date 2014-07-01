
class BaseParser {
public:
	BaseParser(int fd) : p_fd(fd),
			p_readPointer(0), p_readLimit(0), p_eof(false) {
		read();
	}

	//---------------------------------------------------------------
	// low level functions
	//---------------------------------------------------------------

	void read() {
		assert(p_readPointer == p_readLimit);
		ssize_t len = ::read(p_fd, p_readBuffer, kReadBufferSize);
		if(len == -1)
			throw std::runtime_error("Could not read input");
		if(len == 0)
			p_eof = true;
		p_readLimit = len;
		p_readPointer = 0;
	}
	void consume() {
		p_readPointer++;
		if(p_readPointer == p_readLimit)
			read();
	}
	bool atEndOfFile() {
		return p_eof;
	}
	char fetch() {
		assert(p_readPointer < p_readLimit);
		return p_readBuffer[p_readPointer];
	}
	
	//---------------------------------------------------------------
	// high level functions
	//---------------------------------------------------------------

	bool checkSpace() {
		return fetch() == ' ' || fetch() == '\t';
	}
	bool checkBreak() {
		return fetch() == '\n' || fetch() == '\r';
	}
	
	void skipSpace() {
		while(!atEndOfFile() && checkSpace())
			consume();
	}
	void skipSpaceOrBreak() {
		while(!atEndOfFile() && (checkSpace() || checkBreak()))
			consume();
	}
	void skipUntilBreakOrEnd() {
		while(!atEndOfFile() && !checkBreak())
			consume();
		if(!atEndOfFile())
			consume();
	}
	
	void forceBreakOrEnd() {
		if(atEndOfFile())
			return;
		if(!checkBreak())
			throw std::runtime_error("Expected end-of-line");
		consume();
	}

	void readWord() {
		p_tempLength = 0;
		while(!atEndOfFile() && !checkSpace() && !checkBreak()) {
			if(p_tempLength >= kTempBufferSize)
				throw std::logic_error("Buffer limit reached");
			p_tempBuffer[p_tempLength++] = fetch();
			consume();
		}
	}	
	bool bufferMatch(const char *string) {
		for(int i = 0; string[i] != 0; i++) {
			if(i >= p_tempLength)
				return false;
			if(string[i] != p_tempBuffer[i])
				return false;
		}
		return true;
	}
	int64_t bufferGetInt() {
		int64_t num = 0;
		bool neg = false;
		int i = 0;
		
		if(p_tempLength == 0)
			throw std::runtime_error("Expected number (empty buffer)");
		if(i < p_tempLength && p_tempBuffer[i] == '-') {
			neg = true;
			i++;
		}
		while(i < p_tempLength) {
			if(!(p_tempBuffer[i] >= '0' && p_tempBuffer[i] <= '9'))
				throw std::runtime_error("Expected number (illegal char)");
			num = num * 10 + (p_tempBuffer[i] - '0');
			i++;
		}
		return neg ? -num : num;
	}
	
private:
	static const int kReadBufferSize = 16 * 1024;
	static const int kTempBufferSize = 1024;
	
	int p_fd;
	
	char p_readBuffer[kReadBufferSize];
	int p_readPointer;
	int p_readLimit;
	bool p_eof;
	
	char p_tempBuffer[kTempBufferSize];
	int p_tempLength;
};

template<typename Hooks>
class CnfParser {
public:
	CnfParser(Hooks &hooks, int fd) : p_hooks(hooks), p_base(fd) { }

	void parse() {
		while(!p_base.atEndOfFile()) {
			p_base.skipSpaceOrBreak();
			if(p_base.atEndOfFile())
				break;
			p_base.readWord();
			
			if(p_base.bufferMatch("c")) {
				p_base.skipUntilBreakOrEnd();
			}else if(p_base.bufferMatch("p")) {
				// read the file type
				p_base.skipSpace();
				p_base.readWord();
				if(!p_base.bufferMatch("cnf"))
					throw std::runtime_error("Illegal cnf file");
				
				// read the number of vars and clauses
				p_base.skipSpace();
				p_base.readWord();
				int num_vars = p_base.bufferGetInt();
				
				p_base.skipSpace();
				p_base.readWord();
				int num_clauses = p_base.bufferGetInt();

				p_hooks.onProblem(num_vars, num_clauses);

				p_base.skipSpace();
				p_base.forceBreakOrEnd();
			}else{
				// the line contains a clause
				std::vector<long> lits;
				while(true) {
					int lit = p_base.bufferGetInt();
					if(lit == 0) {
						p_base.skipSpace();
						p_base.forceBreakOrEnd();
						break;
					}
					lits.push_back(lit);
					p_base.skipSpaceOrBreak();
					p_base.readWord();
				}
				if(lits.size() == 0)
					throw std::runtime_error("File contains empty clause");
				
				p_hooks.onClause(lits);
			}
		}
	}
private:
	Hooks &p_hooks;
	BaseParser p_base;
};

