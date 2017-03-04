/** Logging and debugging routines
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/
 
#pragma once

#include <stdarg.h>
#include <libconfig.h>

#include "utils.h"

#ifdef __GNUC__
  #define INDENT	int __attribute__ ((__cleanup__(log_outdent), unused)) _old_indent = log_indent(1);
#else
  #define INDENT	;
#endif

/* The log level which is passed as first argument to print() */
#define LOG_LVL_DEBUG	GRY("Debug")
#define LOG_LVL_INFO	WHT("Info ") 
#define LOG_LVL_WARN	YEL("Warn ")
#define LOG_LVL_ERROR	RED("Error")
#define LOG_LVL_STATS	MAG("Stats")

/** Debug facilities.
 *
 * To be or-ed with the debug level
 */
enum log_facilities {
	LOG_POOL =	(1L <<  8),
	LOG_QUEUE =	(1L <<  9),
	LOG_CONFIG =	(1L << 10),
	LOG_HOOK =	(1L << 11),
	LOG_PATH =	(1L << 12),
	LOG_MEM =	(1L << 13),
	LOG_WEB =	(1L << 14),
	LOG_API =	(1L << 15),
	LOG_LOG =	(1L << 16),
	LOG_KERNEL =	(1L << 17),
	
	/* Node-types */
	LOG_SOCKET =	(1L << 32),
	LOG_FILE =	(1L << 33),
	LOG_FPGA =	(1L << 34),
	LOG_NGSI =	(1L << 35),
	LOG_WEBSOCKET =	(1L << 36),
	LOG_OPAL =	(1L << 37),
	
	/* Classes */
	LOG_NODE =   (0xFFL << 32),
	LOG_ALL =    ~0xFF 
};

struct log {
	struct timespec epoch;	/**< A global clock used to prefix the log messages. */

	/** Debug level used by the debug() macro.
	 * It defaults to V (defined by the Makefile) and can be
	 * overwritten by the 'debug' setting in the configuration file. */
	int level;

	/** Debug facilities used by the debug() macro. */
	int facilities;
};

/** Initialize log object */
int log_init(struct log *l);

/** Destroy log object */
int log_destroy(struct log *l);

/** Change log indention  for current thread.
 *
 * The argument level can be negative!
 */
int log_indent(int levels);

/** A helper function the restore the previous log indention level.
 *
 * This function is usually called by a __cleanup__ handler (GCC C Extension).
 * See INDENT macro.
 */
void log_outdent(int *);

/** Set logging facilities based on expression.
 *
 * Currently we support two types of expressions:
 *  1. A comma seperated list of logging facilities
 *  2. A comma seperated list of logging facilities which is prefixes with an exclamation mark '!'
 *
 * The first case enables only faciltities which are in the list.
 * The second case enables all faciltities with exception of those which are in the list.
 * 
 * @param expression The expression
 * @return The new facilties mask (see enum log_faciltities)
 */
int log_set_facility_expression(struct log *l, const char *expression);

/** Parse logging configuration. */
int log_parse(struct log *l, config_setting_t *cfg);

/** Logs variadic messages to stdout.
 *
 * @param lvl The log level
 * @param fmt The format string (printf alike)
 */
void log_print(struct log *l, const char *lvl, const char *fmt, ...)
	__attribute__ ((format(printf, 3, 4)));

/** Logs variadic messages to stdout.
 *
 * @param lvl The log level
 * @param fmt The format string (printf alike)
 * @param va The variadic argument list (see stdarg.h)
 */	
void log_vprint(struct log *l, const char *lvl, const char *fmt, va_list va);

/** Printf alike debug message with level. */
void debug(long lvl, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

/** Print a horizontal line. */
void line();

/** Printf alike info message. */
void info(const char *fmt, ...)
	__attribute__ ((format(printf, 1, 2)));

/** Printf alike warning message. */
void warn(const char *fmt, ...)
	__attribute__ ((format(printf, 1, 2)));

/** Printf alike statistics message. */
void stats(const char *fmt, ...)
	__attribute__ ((format(printf, 1, 2)));

/** Print error and exit. */
void error(const char *fmt, ...)
	__attribute__ ((format(printf, 1, 2)));

/** Print error and strerror(errno). */
void serror(const char *fmt, ...)
	__attribute__ ((format(printf, 1, 2)));

/** Print configuration error and exit. */
void cerror(config_setting_t *cfg, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));