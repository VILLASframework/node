/** Various helper functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>
#include <sched.h>
#include <assert.h>

#include "log.h"

#ifdef __GNUC__
  #define EXPECT(x, v)	__builtin_expect(x, v)
#else
  #define EXPECT(x, v)	(x)
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

#define ALIGN(x, a)	 ALIGN_MASK(x, (uintptr_t) (a) - 1)
#define ALIGN_MASK(x, m) (((uintptr_t) (x) + (m)) & ~(m))
#define IS_ALIGNED(x, a) (ALIGN(x, a) == (uintptr_t) x)

#define CEIL(x, y)	((x + y - 1) / y)

/** Calculate the number of elements in an array. */
#define ARRAY_LEN(a)	( sizeof (a) / sizeof (a)[0] )

/** Swap two values by using a local third one. */
#define SWAP(a, b)	do { \
	 			__typeof__(a) tmp = a; \
				a = b; \
				b = tmp; \
			} while(0)

/* Return the bigger value */
#define MAX(a, b)	({ __typeof__ (a) _a = (a); \
			   __typeof__ (b) _b = (b); \
			   _a > _b ? _a : _b; })

/* Return the smaller value */
#define MIN(a, b)	({ __typeof__ (a) _a = (a); \
			   __typeof__ (b) _b = (b); \
			   _a < _b ? _a : _b; })

/* Forward declarations */
struct settings;
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

/** Format a struct timespec date similar to strftime() */
int strftimespec(char *s, size_t max, const char *format, struct timespec *ts)
	__attribute__ ((format(strftime, 3, 0)));

/** Convert integer to cpu_set_t.
 *
 * @param set A cpu bitmask
 * @return The opaque cpu_set_t datatype
 */
cpu_set_t integer_to_cpuset(int set);

#ifdef WITH_JANSSON
  #include <jansson.h>

/* Convert a libconfig object to a libjansson object */
json_t * config_to_json(config_setting_t *cfg);

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
int version_compare(struct version *a, struct version *b);

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

#endif /* _UTILS_H_ */

