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
#include <string.h>

#include <sched.h>

#ifdef __GNUC__
 #define EXPECT(x, v)	__builtin_expect(x, v)
#else
 #define EXPECT(x, v)	(x)
#endif

/* Some color escape codes for pretty log messages */
#define RED(str)	"\x1B[31m" str "\x1B[0m" /**< Print str in red */
#define GRN(str)	"\x1B[32m" str "\x1B[0m" /**< Print str in green */
#define YEL(str)	"\x1B[33m" str "\x1B[0m" /**< Print str in yellow */
#define BLU(str)	"\x1B[34m" str "\x1B[0m" /**< Print str in blue */
#define MAG(str)	"\x1B[35m" str "\x1B[0m" /**< Print str in magenta */
#define CYN(str)	"\x1B[36m" str "\x1B[0m" /**< Print str in cyan */
#define WHT(str)	"\x1B[37m" str "\x1B[0m" /**< Print str in white */
#define BLD(str)	"\x1B[1m"  str "\x1B[0m" /**< Print str in bold */ 

/** The log level which is passed as first argument to print() */
enum log_level { DEBUG, INFO, WARN, ERROR };

/* Forward declarations */
struct settings;
struct sockaddr_in;
struct sockaddr;

extern int debug;

/** Logs variadic messages to stdout.
 *
 * @param lvl The log level
 * @param fmt The format string (printf alike)
 */
void print(enum log_level lvl, const char *fmt, ...);

/** Resolve host/service name by local databases and/or nameservers.
 *
 * @param addr A string containing the hostname/ip and port seperated by a colon
 * @param sa A pointer to the resolved address
 * @param flags Flags for gai
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int resolve_addr(const char *addr, struct sockaddr_in *sa, int flags);

/** Convert integer to cpu_set_t.
 *
 * @param set A cpu bitmask
 * @return The opaque cpu_set_t datatype
 */
cpu_set_t to_cpu_set(int set);

/** Get delta between two timespec structs */
double timespec_delta(struct timespec *start, struct timespec *end);

/** Get period as timespec from rate */
struct timespec timespec_rate(double rate);

/** Print ASCII style plot of histogram */
void hist_plot(unsigned *hist, int length);

/** Dump histogram data in Matlab format */
void hist_dump(unsigned *hist, int length);

/** Append an element to a single linked list */
#define list_add(list, elm) do { \
		elm->next = list; \
		list = elm; \
	} while (0)

/** Check assertion and exit if failed. */
#define assert(exp) do { \
	if (EXPECT(!exp, 0)) { \
		print(ERROR, "Assertion failed: '%s' in %s, %s:%d", \
			#exp, __FUNCTION__, __BASE_FILE__, __LINE__); \
		exit(EXIT_FAILURE); \
	} } while (0)

/** Printf alike debug message with level. */
#define debug(lvl, msg, ...) do { \
	if (lvl <= debug) \
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
#define perror(msg, ...) do { \
		print(ERROR, msg ": %s", ##__VA_ARGS__, \
			strerror(errno)); \
		exit(EXIT_FAILURE); \
	} while (0)

/** Print configuration error and exit. */
#define cerror(c, msg, ...) do { \
		print(ERROR, msg " in %s:%u", ##__VA_ARGS__, \
			config_setting_source_file(c), \
			config_setting_source_line(c)); \
		exit(EXIT_FAILURE); \
	} while (0)

#endif /* _UTILS_H_ */
