/** Various helper functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>

#include <villas/config.h>
#include <villas/log.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __GNUC__
  #define LIKELY(x)	__builtin_expect((x),1)
  #define UNLIKELY(x)	__builtin_expect((x),0)
#else
  #define LIKELY(x)	(x)
  #define UNLIKELY(x)	(x)
#endif

/* Some color escape codes for pretty log messages */
#define CLR(clr, str)	"\e[" XSTR(clr) "m" str "\e[0m"
#define CLR_GRY(str)	CLR(30, str) /**< Print str in gray */
#define CLR_RED(str)	CLR(31, str) /**< Print str in red */
#define CLR_GRN(str)	CLR(32, str) /**< Print str in green */
#define CLR_YEL(str)	CLR(33, str) /**< Print str in yellow */
#define CLR_BLU(str)	CLR(34, str) /**< Print str in blue */
#define CLR_MAG(str)	CLR(35, str) /**< Print str in magenta */
#define CLR_CYN(str)	CLR(36, str) /**< Print str in cyan */
#define CLR_WHT(str)	CLR(37, str) /**< Print str in white */
#define CLR_BLD(str)	CLR( 1, str) /**< Print str in bold */

/* Alternate character set
 *
 * The suffixed of the BOX_ macro a constructed by
 * combining the following letters in the written order:
 *   - U for a line facing upwards
 *   - D for a line facing downwards
 *   - L for a line facing leftwards
 *   - R for a line facing rightwards
 *
 * E.g. a cross can be constructed by combining all line fragments:
 *    BOX_UDLR
 */
#define BOX(chr)	"\e(0" chr "\e(B"
#define BOX_LR		BOX("\x71") /**< Boxdrawing: ─ */
#define BOX_UD		BOX("\x78") /**< Boxdrawing: │ */
#define BOX_UDR		BOX("\x74") /**< Boxdrawing: ├ */
#define BOX_UDLR	BOX("\x6E") /**< Boxdrawing: ┼ */
#define BOX_UDL		BOX("\x75") /**< Boxdrawing: ┤ */
#define BOX_ULR		BOX("\x76") /**< Boxdrawing: ┴ */
#define BOX_UL		BOX("\x6A") /**< Boxdrawing: ┘ */
#define BOX_DLR		BOX("\x77") /**< Boxdrawing: ┘ */
#define BOX_DL		BOX("\x6B") /**< Boxdrawing: ┘ */

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

/** Print copyright message to stdout. */
void print_copyright();

/** Print version to stdout. */
void print_version();

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

#ifdef __linux__
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
#endif

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

/** Get log2 of long long integers */
static inline int log2i(long long x) {
	if (x == 0)
		return 1;

	return sizeof(x) * 8 - __builtin_clzll(x) - 1;
}

/** Register a exit callback for program termination: SIGINT, SIGKILL & SIGALRM. */
int signals_init(void (*cb)(int signal, siginfo_t *sinfo, void *ctx));

/** Send signal \p sig to main thread. */
void killme(int sig);

pid_t spawn(const char *name, char *const argv[]);

/** Determines the string length as printed on the screen (ignores escable sequences). */
size_t strlenp(const char *str);


#ifdef __cplusplus
}
#endif
