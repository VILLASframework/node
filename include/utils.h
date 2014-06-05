/**
 * Some helper functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdarg.h>

#include <netinet/ip.h>

enum log_level
{
	DEBUG,
	INFO,
	WARN,
	ERROR
};

/**
 * @brief Logs variadic messages to stdout
 * @param lvl The log level
 * @param fmt The format string (printf alike)
 */
void print(enum log_level lvl, const char *fmt, ...);

/**
 * @brief Resolve host/service name by local databases and/or nameservers
 *
 * @param addr A string containing the hostname/ip and port seperated by a colon
 * @param sa A pointer to the resolved address
 * @return
 * - 0 on success
 */
int resolve(const char *addr, struct sockaddr_in *sa);

#define assert(exp) do { \
	if (!(exp)) { \
		print(ERROR, "Assertion failed: '%s' in %s, %s:%d", \
			#exp, __FUNCTION__, __BASE_FILE__, __LINE__); \
		exit(EXIT_FAILURE); \
	} } while (0)

#define debug(lvl, msg, ...) do { \
	if (lvl <= V) \
		print(DEBUG, msg, ##__VA_ARGS__); \
	} while (0)

#define info(msg, ...) do { \
		print(INFO, msg, ##__VA_ARGS__); \
	} while (0)

#define warn(msg, ...) do { \
		print(WARN, msg, ##__VA_ARGS__); \
	} while (0)

#define error(msg, ...) do { \
		print(ERROR, msg, ##__VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while (0)

#endif /* _UTILS_H_ */

