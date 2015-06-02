/** Various helper functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

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

/* UTF-8 Line drawing characters */
#define UTF8_BOX	"\xE2\x96\x88"
#define UTF8_VERTICAL	"\xE2\x94\x82"
#define UTF8_VERTRIGHT	"\xE2\x94\x9C"

/* CPP stringification */
#define XSTR(x)		STR(x)
#define  STR(x)		#x

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

/** The main thread id.
 * This is used to notify the main thread about
 * the program termination.
 * See error() macros.
 */
extern pthread_t _mtid;

/** Normal random variate generator using the Box-Muller method
 *
 * @param m Mean
 * @param s Standard deviation
 * @return Normal variate random variable (Gaussian)
 */
double box_muller(float m, float s);

/** Double precission uniform random variable */
double randf();

/** Safely append a format string to an existing string.
 *
 * This function is similar to strlcat() from BSD.
 */
int strap(char *dest, size_t size, const char *fmt,  ...);

/** Variadic version of strap() */
int vstrap(char *dest, size_t size, const char *fmt, va_list va);

/** Convert integer to cpu_set_t.
 *
 * @param set A cpu bitmask
 * @return The opaque cpu_set_t datatype
 */
cpu_set_t to_cpu_set(int set);

/** Allocate and initialize memory. */
void * alloc(size_t bytes);

/** Wait until timer elapsed
 *
 * @retval 0 An error occured. Maybe the timer was stopped.
 * @retval >0 The nummer of runs this timer already fired.
 */
uint64_t timerfd_wait(int fd);

/** Get delta between two timespec structs */
double timespec_delta(struct timespec *start, struct timespec *end);

/** Get period as timespec from rate */
struct timespec timespec_rate(double rate);

/** A system(2) emulator with popen / pclose(2) and proper output handling */
int system2(const char* cmd, ...);

/** Call quit() in the main thread. */
void die();

/** Check assertion and exit if failed. */
#define assert(exp) do { \
	if (!EXPECT(exp, 0)) \
		error("Assertion failed: '%s' in %s(), %s:%d", \
			XSTR(exp), __FUNCTION__, __BASE_FILE__, __LINE__); \
	} while (0)

#endif /* _UTILS_H_ */

