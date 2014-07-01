
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace sys {

class Reporter {
private:
	unsigned int kChunkSize = 4 * 1024 * 1024;
	
public:
	Reporter() : p_fd(-1) {
	}
	
	~Reporter() {
		if(p_fd != -1)
			close();
	}
	
	void open(int id) {
		assert(p_fd == -1);
		
		std::string fname;
		fname += std::to_string(getpid());
		fname += "-";
		fname += std::to_string(id);
		fname += ".rpt";
		int res = ::open(fname.c_str(), O_CREAT | O_APPEND | O_TRUNC | O_WRONLY, 0660);
		if(res == -1)
			throw std::runtime_error("Could not open report file");
		p_fd = res;
		p_buffer = new uint8_t[kChunkSize];

	}
	
	void close() {
		assert(p_fd != -1);
		
		submit();
		if(::close(p_fd) == -1)
			throw std::runtime_error("Could not close report file");
		p_fd = -1;
		delete[] p_buffer;
	}

	template<typename T>
	void write(const T &data) {
		assert(p_fd != -1);
		
		if(p_offset + sizeof(data) >= kChunkSize)
			submit();
		assert(p_offset + sizeof(data) < kChunkSize);
		std::memcpy(p_buffer + p_offset, &data, sizeof(data));
		p_offset += sizeof(data);
	}

	void submit() {
		ssize_t written = 0;
		while(written < p_offset) {
			int res = ::write(p_fd, p_buffer + written, p_offset - written);
			if(res == -1)
				throw std::runtime_error("Could not write to report file");
			written += res;
		}
		p_offset = 0;
	}
	
private:
	int p_fd;
	uint8_t *p_buffer;
	uint32_t p_offset;
};

}; // namespace sys

