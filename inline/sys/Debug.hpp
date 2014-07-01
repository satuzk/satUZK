
static const int SYS_DBG4 = 4;
static const int SYS_DBG3 = 3;
static const int SYS_DBG2 = 2;
static const int SYS_DBG1 = 1;
static const int SYS_DBG0 = 0;

static const int SYS_LEVEL = 0;

static const int SYS_ASSERT_LEVEL = 4;

static const bool SYS_DBG_WATCH = true;
static const bool SYS_DBG_PROPAGATE = true;

static const bool SYS_DBG_SOLVE = false;
static const bool SYS_DBG_ASSIGN = false;
static const bool SYS_DBG_ROLLBACK = false;
static const bool SYS_DBG_BACKTRACK = false;
static const bool SYS_DBG_LEARNING = false;
static const bool SYS_DBG_1UIP = false;
static const bool SYS_DBG_LEARN_CLAUSE = false;
static const bool SYS_DBG_LEARN_MINIMIZE = false;
static const bool SYS_DBG_REMOVE = false;

static const bool SYS_ASRT_ALL = true;

static const bool SYS_ASRT_GENERAL = true;
static const bool SYS_ASRT_SOLVE = true;
static const bool SYS_ASRT_ASSIGN = true;
static const bool SYS_ASRT_PROPAGATE = true;
static const bool SYS_ASRT_ROLLBACK = true;
static const bool SYS_ASRT_CHAIN = true;
static const bool SYS_ASRT_BOOL = false;
static const bool SYS_ASRT_SDHE = true;
static const bool SYS_ASRT_ANTECEDENT = true;
static const bool SYS_ASRT_LEARNING = true;
static const bool SYS_ASRT_WATCH = true;

static const bool SYS_CRITICAL_WARNINGS = true;
static const bool SYS_WARN_SOLVE = true;
static const bool SYS_WARN_REMOVE = true;

#ifdef SYS_NO_DEBUG
	static const bool SYS_ASRT_ENABLE = false;
#else
	static const bool SYS_ASRT_ENABLE = true;
#endif

#define SYS_ASSERT(OPTION, COND) do { if(SYS_ASRT_ENABLE \
	&& (OPTION || SYS_ASRT_ALL) \
	&& !(COND)) sysAssertFail(#OPTION, __FILE__, __LINE__, \
	__PRETTY_FUNCTION__, #COND); } while(false)

#define SYS_DEBUG(OPTION, FORMAT, ...) do { if(OPTION) \
	sysPerformDebug(#OPTION, FORMAT, ##__VA_ARGS__); } while(false)

#define SYS_WARNING(OPTION, FORMAT, ...) do { if(OPTION) \
	sysPerformWarning(#OPTION, FORMAT, ##__VA_ARGS__); } while(false)

#define SYS_CRITICAL(FORMAT, ...) do { sysPerformCritical(__FILE__, \
	__LINE__, __PRETTY_FUNCTION__, FORMAT, ##__VA_ARGS__); } while(false)

bool sysLevel(int level);

void sysPrintError(const char *message, ...)
		__attribute__(( format(printf, 1, 2) ));

void sysAssertFail(const char *option, const char *file, const int line,
		const char *function, const char *condition);
void sysPerformDebug(const char *option, const char *message, ...)
		__attribute__(( format(printf, 2, 3) ));
void sysPerformWarning(const char *option, const char *message, ...)
		__attribute__(( format(printf, 2, 3) ));

void sysCritical(const char *message, ...) __attribute__(( noreturn,
		format(printf, 1, 2) ));

void sysPerformCritical(const char *file, const int line,
		const char *function, const char *message, ...)
	__attribute__ (( noreturn, format(printf, 4, 5) ));

void *sysPageAllocate(std::size_t length);
void sysPageDeallocate(void *pointer, std::size_t length);

typedef int64_t sys_msecs_t;

sys_msecs_t sysGetWallTime();
sys_msecs_t sysGetCpuTime();
uint64_t sysGetUsedMemory();

int sysPeakMemory();

#include <iostream>

