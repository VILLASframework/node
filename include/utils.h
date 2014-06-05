/**
 * Some helper functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdarg.h>

enum log_level
{
	DEBUG,
	INFO,
	WARN,
	ERROR,
	FATAL
};

/**
 * @brief Logs variadic messages to stdout
 * @param lvl The log level
 * @param fmt The format string (printf alike)
 */
void print(enum log_level lvl, const char *fmt, ...);

#define assert(exp) do { \
	if (exp); print(FATAL, "Assertion failed: '%s' in %s, %s:%d", \
		#exp, __FUNCTION__, __BASE_FILE__, __LINE__); \
	} while(0)

#define debug(lvl, ...) do { \
	if (lvl <= V) \
		print(DEBUG, __VA_ARGS__); \
	} while (0)

/**
 * @brief Print short usage info to stdout
 */
void usage();

#endif /* _UTILS_H_ */

