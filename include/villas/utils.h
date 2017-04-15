/** Various helper functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>

#include <openssl/sha.h>

#include "log.h"

#ifdef __GNUC__
  #define LIKELY(x)	__builtin_expect((x),1)
  #define UNLIKELY(x)	__builtin_expect((x),0)
#else
  #define LIKELY(x)	(x)
  #define UNLIKELY(x)	(x)
#endif

/* Some color escape codes for pretty log messages */
#define GRY(str)	"\e[30m" str "\e[0m" /**< Print str in gray */
#define RED(str)	"\e[31m" str "\e[0m" /**< Print str in red */
#define GRN(str)	"\e[32m" str "\e[0m" /**< Print str in green */
#define YEL(str)	"\e[33m" str "\e[0m" /**< Print str in yellow */
#define BLU(str)	"\e[34m" str "\e[0m" /**< Print str in blue */
#define MAG(str)	"\e[35m" str "\e[0m" /**< Print str in magenta */
#define CYN(str)	"\e[36m" str "\e[0m" /**< Print str in cyan */
#define WHT(str)	"\e[37m" str "\e[0m" /**< Print str in white */
#define BLD(str)	"\e[1m"  str "\e[0m" /**< Print str in bold */

/* Alternate character set */
#define ACS(chr)	"\e(0" chr "\e(B"
#define ACS_HORIZONTAL	ACS("\x71")
#define ACS_VERTICAL	ACS("\x78")
#define ACS_VERTRIGHT	ACS("\x74")

/* CPP stringification */
#define XSTR(x)		STR(x)
#define  STR(x)		#x

#define CONCAT_DETAIL(x, y)	x##y
#define CONCAT(x, y)		CONCAT_DETAIL(x, y)
#define UNIQUE(x)		CONCAT(x, __COUNTER__)

#define ALIGN(x, a)	 ALIGN_MASK(x, (uintptr_t) (a) - 1)
#define ALIGN_MASK(x, m) (((uintptr_t) (x) + (m)) & ~(m))
#define IS_ALIGNED(x, a) (ALIGN(x, a) == (uintptr_t) x)

#define SWAP(x,y) do {	\
  __auto_type _x = x;	\
  __auto_type _y = y;	\
  x = _y;		\
  y = _x;		\
} while(0)

/** Round-up integer division */
#define CEIL(x, y)	(((x) + (y) - 1) / (y))

/** Get nearest up-rounded power of 2 */
#define LOG2_CEIL(x)	(1 << (log2i((x) - 1) + 1))

/** Check if the number is a power of 2 */
#define IS_POW2(x)	(((x) != 0) && !((x) & ((x) - 1)))

/** Calculate the number of elements in an array. */
#define ARRAY_LEN(a)	( sizeof (a) / sizeof (a)[0] )

/* Return the bigger value */
#define MAX(a, b)	({ __typeof__ (a) _a = (a); \
			   __typeof__ (b) _b = (b); \
			   _a > _b ? _a : _b; })

/* Return the smaller value */
#define MIN(a, b)	({ __typeof__ (a) _a = (a); \
			   __typeof__ (b) _b = (b); \
			   _a < _b ? _a : _b; })

#ifndef offsetof
  #define offsetof(type, member)  __builtin_offsetof(type, member)
#endif

#ifndef container_of
  #define container_of(ptr, type, member) ({ const typeof( ((type *) 0)->member ) *__mptr = (ptr); \
					  (type *) ( (char *) __mptr - offsetof(type, member) ); \
					})
#endif

#define BITS_PER_LONGLONG	(sizeof(long long) * 8)

/* Some helper macros */
#define BITMASK(h, l)		(((~0ULL) << (l)) & (~0ULL >> (BITS_PER_LONGLONG - 1 - (h))))
#define BIT(nr)			(1UL << (nr))

/* Forward declarations */
struct timespec;

/** Print copyright message to screen. */
void print_copyright();

/** Normal random variate generator using the Box-Muller method
 *
 * @param m Mean
 * @param s Standard deviation
 * @return Normal variate random variable (Gaussian)
 */
double box_muller(float m, float s);

/** Double precission uniform random variable */
double randf();

/** Concat formatted string to an existing string.
 *
 * This function uses realloc() to resize the destination.
 * Please make sure to only on dynamic allocated destionations!!!
 *
 * @param dest A pointer to a malloc() allocated memory region
 * @param fmt A format string like for printf()
 * @param ... Optional parameters like for printf()
 * @retval The the new value of the dest buffer.
 */
char * strcatf(char **dest, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

/** Variadic version of strcatf() */
char * vstrcatf(char **dest, const char *fmt, va_list va)
	__attribute__ ((format(printf, 2, 0)));

/** Format string like strcatf() just starting with empty string */
#define strf(fmt, ...) strcatf(&(char *) { NULL }, fmt, ##__VA_ARGS__)
#define vstrf(fmt, va) vstrcatf(&(char *) { NULL }, fmt, va)

/** Convert integer to cpu_set_t.
 *
 * @param set An integer number which is used as the mask
 * @param cset A pointer to the cpu_set_t datastructure
 */
void cpuset_from_integer(uintmax_t set, cpu_set_t *cset);

/** Convert cpu_set_t to an integer. */
void cpuset_to_integer(cpu_set_t *cset, uintmax_t *set);

/** Parses string with list of CPU ranges.
 *
 * From: https://github.com/mmalecki/util-linux/blob/master/lib/cpuset.c
 *
 * @retval 0 On success.
 * @retval 1 On error.
 * @retval 2 If fail is set and a cpu number passed in the list doesn't fit
 * into the cpu_set. If fail is not set cpu numbers that do not fit are
 * ignored and 0 is returned instead.
 */
int cpulist_parse(const char *str, cpu_set_t *set, int fail);

/** Returns human readable representation of the cpuset.
 *
 * From: https://github.com/mmalecki/util-linux/blob/master/lib/cpuset.c
 *
 * The output format is a list of CPUs with ranges (for example, "0,1,3-9").
 */
char * cpulist_create(char *str, size_t len, cpu_set_t *set);

/** Allocate and initialize memory. */
void * alloc(size_t bytes);

/** Allocate and copy memory. */
void * memdup(const void *src, size_t bytes);

/** Call quit() in the main thread. */
void die();

/** Used by version_parse(), version_compare() */
struct version {
	int major;
	int minor;
};

/** Compare two versions. */
int version_cmp(struct version *a, struct version *b);

/** Parse a dotted version string. */
int version_parse(const char *s, struct version *v);

/** Check assertion and exit if failed. */
#ifndef assert
  #define assert(exp) do { \
	if (!EXPECT(exp, 0)) \
		error("Assertion failed: '%s' in %s(), %s:%d", \
			XSTR(exp), __FUNCTION__, __BASE_FILE__, __LINE__); \
	} while (0)
#endif

/** Fill buffer with random data */
ssize_t read_random(char *buf, size_t len);

/** Get CPU timestep counter */
__attribute__((always_inline)) static inline uint64_t rdtsc()
{
	uint64_t tsc;

	__asm__ ("rdtsc;"
		 "shl $32, %%rdx;"
		 "or %%rdx,%%rax"
		: "=a" (tsc)
		:
		: "%rcx", "%rdx", "memory");

	return tsc;
}

/** Get log2 of long long integers */
static inline int log2i(long long x) {
	if (x == 0)
		return 1;

	return sizeof(x) * 8 - __builtin_clzll(x) - 1;
}

/** Sleep with rdtsc */
void rdtsc_sleep(uint64_t nanosecs, uint64_t start);

/** Register a exit callback for program termination (SIGINT / SIGKILL). */
void signals_init(void (*cb)(int signal, siginfo_t *sinfo, void *ctx));

pid_t spawn(const char* name, char **argv);
