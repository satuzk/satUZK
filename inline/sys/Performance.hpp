

namespace sys {

typedef uint64_t HptCounter;

//#define UTIL_PERFORMANCE_GETTIMEOFDAY
#ifdef UTIL_PERFORMANCE_GETTIMEOFDAY
#include <sys/time.h>
inline HptCounter hptCurrent() {
	struct timeval time_struct;
	if(gettimeofday(&time_struct, NULL) != 0)
		SYS_CRITICAL("gettimeofday() failed\n");
	return time_struct.tv_sec * 1000LL * 1000LL * 1000LL + time_struct.tv_usec * 1000LL;
}
#else
#include <time.h>
inline HptCounter hptCurrent() {
	timespec time_struct;
	if(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time_struct) != 0)
		SYS_CRITICAL("clock_gettime() failed\n");
	return time_struct.tv_sec * 1000LL * 1000LL * 1000LL + time_struct.tv_nsec;
}
#endif

inline HptCounter hptElapsed(HptCounter since) {
	return hptCurrent() - since;
}

};

