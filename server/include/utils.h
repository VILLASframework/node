/** Various helper functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sched.h>
#include <string.h>
#include <sys/types.h>

#include "log.h"

#ifdef __GNUC__
 #define EXPECT(x, v)	__builtin_expect(x, v)
#else
 #define EXPECT(x, v)	(x)
#endif

/* Some color escape codes for pretty log messages */
#ifndef ENABLE_OPAL_ASYNC
 #define GRY(str)	"\e[30m" str "\e[0m" /**< Print str in gray */
 #define RED(str)	"\e[31m" str "\e[0m" /**< Print str in red */
 #define GRN(str)	"\e[32m" str "\e[0m" /**< Print str in green */
 #define YEL(str)	"\e[33m" str "\e[0m" /**< Print str in yellow */
 #define BLU(str)	"\e[34m" str "\e[0m" /**< Print str in blue */
 #define MAG(str)	"\e[35m" str "\e[0m" /**< Print str in magenta */
 #define CYN(str)	"\e[36m" str "\e[0m" /**< Print str in cyan */
 #define WHT(str)	"\e[37m" str "\e[0m" /**< Print str in white */
 #define BLD(str)	"\e[1m"  str "\e[0m" /**< Print str in bold */

 #define GFX(chr)	"\e(0" chr "\e(B"
 #define UP(n)		"\e[" ## n ## "A"
 #define DOWN(n)	 "\e[" ## n ## "B"
 #define RIGHT(n)	"\e[" ## n ## "C"
 #define LEFT(n)	 "\e[" ## n ## "D"
#else
 #define GRY(str)	str
 #define RED(str)	str
 #define GRN(str)	str
 #define YEL(str)	str
 #define BLU(str)	str
 #define MAG(str)	str
 #define CYN(str)	str
 #define WHT(str)	str
 #define BLD(str)	str

 #define GFX(chr)	" "
 #define UP(n)		""
 #define DOWN(n)	""
 #define RIGHT(n)	""
 #define LEFT(n)	""
#endif

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

/* Forward declarations */
struct settings;
struct timespec;

/** The main thread id.
 * This is used to notify the main thread about
 * the program termination.
 * See error() macros.
 */
extern pthread_t _mtid;

/** Safely append a format string to an existing string.
 *
 * This function is similar to strlcat() from BSD.
 */
int strap(char *dest, size_t size, const char *fmt,  ...);

/** Variable arguments (stdarg) version of strap() */
int vstrap(char *dest, size_t size, const char *fmt, va_list va);

/** Convert integer to cpu_set_t.
 *
 * @param set A cpu bitmask
 * @return The opaque cpu_set_t datatype
 */
cpu_set_t to_cpu_set(int set);

/** Allocate and initialize memory. */
void * alloc(size_t bytes);

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
	if (EXPECT(!exp, 0)) { \
		print(ERROR, "Assertion failed: '%s' in %s, %s:%d", \
			#exp, __FUNCTION__, __BASE_FILE__, __LINE__); \
		exit(EXIT_FAILURE); \
	} } while (0)

#endif /* _UTILS_H_ */

