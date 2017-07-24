/** Logging and debugging routines
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <time.h>
#include <sys/ioctl.h>

#include "common.h"
#include "log_config.h"

#ifdef __GNUC__
  #define INDENT	int __attribute__ ((__cleanup__(log_outdent), unused)) _old_indent = log_indent(1);
  #define NOINDENT	int __attribute__ ((__cleanup__(log_outdent), unused)) _old_indent = log_noindent();
#else
  #define INDENT	;
  #define NOINDENT	;
#endif

/* The log level which is passed as first argument to print() */
#define LOG_LVL_DEBUG	CLR_GRY("Debug")
#define LOG_LVL_INFO	CLR_WHT("Info ")
#define LOG_LVL_WARN	CLR_YEL("Warn ")
#define LOG_LVL_ERROR	CLR_RED("Error")
#define LOG_LVL_STATS	CLR_MAG("Stats")

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
	LOG_NODE =	(1L << 13),
	LOG_MEM =	(1L << 14),
	LOG_WEB =	(1L << 15),
	LOG_API =	(1L << 16),
	LOG_LOG =	(1L << 17),
	LOG_VFIO =	(1L << 18),
	LOG_PCI =	(1L << 19),
	LOG_XIL =	(1L << 20),
	LOG_TC =	(1L << 21),
	LOG_IF =	(1L << 22),
	LOG_ADVIO =	(1L << 23),

	/* Node-types */
	LOG_SOCKET =	(1L << 24),
	LOG_FILE =	(1L << 25),
	LOG_FPGA =	(1L << 26),
	LOG_NGSI =	(1L << 27),
	LOG_WEBSOCKET =	(1L << 28),
	LOG_OPAL =	(1L << 30),

	/* Classes */
	LOG_NODES =	LOG_NODE | LOG_SOCKET | LOG_FILE | LOG_FPGA | LOG_NGSI | LOG_WEBSOCKET | LOG_OPAL,
	LOG_KERNEL =	LOG_VFIO | LOG_PCI | LOG_TC | LOG_IF,
	LOG_ALL =	~0xFF
};

struct log {
	enum state state;

	struct timespec epoch;	/**< A global clock used to prefix the log messages. */

	struct winsize window;	/**< Size of the terminal window. */

	/** Debug level used by the debug() macro.
	 * It defaults to V (defined by the Makefile) and can be
	 * overwritten by the 'debug' setting in the configuration file. */
	int level;
	long facilities;	/**< Debug facilities used by the debug() macro. */
	const char *path;	/**< Path of the log file. */
	FILE *file;		/**< Send all log output to this file / stdout / stderr. */
};

/** The global log instance. */
struct log *global_log;
struct log default_log;

/** Initialize log object */
int log_init(struct log *l, int level, long faciltities);

int log_start(struct log *l);

int log_stop(struct log *l);

/** Destroy log object */
int log_destroy(struct log *l);

/** Change log indention  for current thread.
 *
 * The argument level can be negative!
 */
int log_indent(int levels);

/** Disable log indention of current thread. */
int log_noindent();

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

/** @addtogroup table Print fancy tables
 * @{
 */

struct table_column {
	int width;	/**< Width of the column. */
	char *title;	/**< The title as shown in the table header. */
	char *format;	/**< The format which is used to print the table rows. */
	char *unit;	/**< An optional unit which will be shown in the table header. */

	enum {
		TABLE_ALIGN_LEFT,
		TABLE_ALIGN_RIGHT
	} align;

	int _width;	/**< The real width of this column. Calculated by table_header() */
};

struct table {
	int ncols;
	int width;
	struct table_column *cols;
};

/** Print a table header consisting of \p n columns. */
void table_header(struct table *t);

/** Print table rows. */
void table_row(struct table *t, ...);

/** Print the table footer. */
void table_footer(struct table *t);

/** @} */

#ifdef __cplusplus
}
#endif
