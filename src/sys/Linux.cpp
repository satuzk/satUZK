
#include <cstdlib>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ctime>

#include <unistd.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include "../../inline/sys/Debug.hpp"

void sysPrintError(const char *format, ...) {
	va_list arguments;
	va_start(arguments, format);
	vprintf(format, arguments);
	va_end(arguments);
}

void sysCritical(const char *format, ...) {
	va_list arguments;
	va_start(arguments, format);
	vprintf(format, arguments);
	va_end(arguments);
	abort();
}

void sysAssertFail(const char *option, const char *file, const int line,
		const char *function, const char *condition) {
	sysCritical("Assertion failed! (Option %s)\n"
			"   In file %s:%d and function '%s'\n"
			"   Assertion: '%s'\n",
		option, file, line, function, condition);
}

void sysPerformDebug(const char *option, const char *format, ...) {
	printf("[%s] ", option);
	va_list arguments;
	va_start(arguments, format);
	vprintf(format, arguments);
	va_end(arguments);
}

void sysPerformWarning(const char *option, const char *format, ...) {
	printf("[%s] ", option);
	va_list arguments;
	va_start(arguments, format);
	vprintf(format, arguments);
	va_end(arguments);
	if(SYS_CRITICAL_WARNINGS)
		abort();
}

void sysPerformCritical(const char *file, const int line,
		const char *function, const char *format, ...) {
	printf("Critical error!\n"
			"   In file %s:%d and function '%s'\n",
			file, line, function);
	va_list arguments;
	va_start(arguments, format);
	vprintf(format, arguments);
	va_end(arguments);
	abort();
}

void *sysPageAllocate(size_t length) {
	void *pointer = mmap(NULL, length, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	SYS_ASSERT(0, pointer != MAP_FAILED);
	return pointer;
}
void sysPageDeallocate(void *pointer, size_t length) {
	int result = munmap(pointer, length);
	SYS_ASSERT(0, result != -1);
}

sys_msecs_t sysGetWallTime() {
	timeval walltime;
	if(gettimeofday(&walltime, NULL) != 0)
		SYS_CRITICAL("gettimeofday() failed\n");
	return walltime.tv_sec * 1000
			+ walltime.tv_usec / 1000;
}

sys_msecs_t sysGetCpuTime() {
	rusage usageinfo;
	if(getrusage(RUSAGE_THREAD, &usageinfo) != 0)
		SYS_CRITICAL("rusage() failed");
	return usageinfo.ru_utime.tv_sec * 1000
			+ usageinfo.ru_utime.tv_usec / 1000;
}

uint64_t sysGetUsedMemory() {
	rusage usageinfo;
	if(getrusage(RUSAGE_THREAD, &usageinfo) != 0)
		SYS_CRITICAL("rusage() failed");
	return (uint64_t)usageinfo.ru_maxrss * 1024;
}

int sysPeakMemory() {
    char  name[256];
    pid_t pid = getpid();

    sprintf(name, "/proc/%d/status", pid);
    FILE* in = fopen(name, "rb");
    if (in == NULL) return 0;

    // Find the correct line, beginning with "VmPeak:":
    int peak_kb = 0;
    while (!feof(in) && fscanf(in, "VmPeak: %d kB", &peak_kb) != 1)
        while (!feof(in) && fgetc(in) != '\n')
            ;
    fclose(in);

    return peak_kb;
}


