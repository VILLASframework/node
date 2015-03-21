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

/** The log level which is passed as first argument to print() */
enum log_level { DEBUG, INFO, WARN, ERROR };

/* Forward declarations */
struct settings;
struct timespec;

/* These global variables allow changing the output style and verbosity */
extern int _debug;
extern int _indent;

void outdent(int *old);

#ifdef __GNUC__
 #define INDENT		int __attribute__ ((__cleanup__(outdent), unused)) _old_indent = _indent++;
#else
 #define INDENT		;
#endif

/** Reset the wallclock of debugging outputs */
void epoch_reset();

/** Logs variadic messages to stdout.
 *
 * @param lvl The log level
 * @param fmt The format string (printf alike)
 */
void print(enum log_level lvl, const char *fmt, ...);

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

/** A system(2) emulator with popen/pclose(2) and proper output handling */
int system2(const char* cmd, ...);

/** Check assertion and exit if failed. */
#define assert(exp) do { \
	if (EXPECT(!exp, 0)) { \
		print(ERROR, "Assertion failed: '%s' in %s, %s:%d", \
			#exp, __FUNCTION__, __BASE_FILE__, __LINE__); \
		exit(EXIT_FAILURE); \
	} } while (0)

/** Printf alike debug message with level. */
#define debug(lvl, msg, ...) do { \
	if (lvl <= _debug) \
		print(DEBUG, msg, ##__VA_ARGS__); \
	} while (0)

/** Printf alike info message. */
#define info(msg, ...) do { \
		print(INFO, msg, ##__VA_ARGS__); \
	} while (0)

/** Printf alike warning message. */
#define warn(msg, ...) do { \
		print(WARN, msg, ##__VA_ARGS__); \
	} while (0)

/** Print error and exit. */
#define error(msg, ...) do { \
		print(ERROR, msg, ##__VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while (0)

/** Print error and strerror(errno). */
#define serror(msg, ...) do { \
		print(ERROR, msg ": %s", ##__VA_ARGS__, \
			strerror(errno)); \
		exit(EXIT_FAILURE); \
	} while (0)

/** Print configuration error and exit. */
#define cerror(c, msg, ...) do { \
		print(ERROR, msg " in %s:%u", ##__VA_ARGS__, \
			(config_setting_source_file(c)) ? \
			 config_setting_source_file(c) : "(stdio)", \
			config_setting_source_line(c)); \
		exit(EXIT_FAILURE); \
	} while (0)

#endif /* _UTILS_H_ */

