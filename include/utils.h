/** Various helper functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file utils.h
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

struct config;
struct sockaddr_in;

/// The log level which is passed as first argument to print()
enum log_level
{
	DEBUG,
	INFO,
	WARN,
	ERROR
};

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
 * @return
 * - 0 on success
 * - otherwise an error occured
 */
int resolve(const char *addr, struct sockaddr_in *sa, int flags);

/** Setup various realtime related things.
 *
 * We use the following techniques for performance tuning
 *  - Prefault the stack
 *  - Lock memory
 *  - Use FIFO scheduler with realtime priority
 *  - Set CPU affinity
 *
 * @param g The global configuration
 */
void realtime_init(struct config *g);

/// Check assertion and exit if failed.
#define assert(exp) do { \
	if (!(exp)) { \
		print(ERROR, "Assertion failed: '%s' in %s, %s:%d", \
			#exp, __FUNCTION__, __BASE_FILE__, __LINE__); \
		exit(EXIT_FAILURE); \
	} } while (0)

/// Printf alike debug message with level.
#define debug(lvl, msg, ...) do { \
	if (lvl <= V) \
		print(DEBUG, msg, ##__VA_ARGS__); \
	} while (0)

/// Printf alike info message.
#define info(msg, ...) do { \
		print(INFO, msg, ##__VA_ARGS__); \
	} while (0)

/// Printf alike warning message.
#define warn(msg, ...) do { \
		print(WARN, msg, ##__VA_ARGS__); \
	} while (0)

/// Print error and exit.
#define error(msg, ...) do { \
		print(ERROR, msg, ##__VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while (0)

/// Print error and strerror(errno).
#define perror(msg, ...) do { \
		print(ERROR, msg ": %s", ##__VA_ARGS__, \
			strerror(errno)); \
		exit(EXIT_FAILURE); \
	} while (0)

/// Print configuration error and exit.
#define cerror(c, msg, ...) do { \
		print(ERROR, msg " in %s:%u", ##__VA_ARGS__, \
			config_setting_source_file(c), \
			config_setting_source_line(c)); \
		exit(EXIT_FAILURE); \
	} while (0)

#endif /* _UTILS_H_ */
