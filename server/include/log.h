/** Logging and debugging routines
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */
 
#ifndef _LOG_H_
#define _LOG_H_

#include <libconfig.h>

#ifdef __GNUC__
 #define INDENT		int __attribute__ ((__cleanup__(log_outdent), unused)) _old_indent = log_indent(1);
#else
 #define INDENT		;
#endif

/* The log level which is passed as first argument to print() */
#define DEBUG		GRY("Debug")
#define INFO		    "" 
#define WARN		YEL("Warn ")
#define ERROR		RED("Error")

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

/** Set the verbosity level of debug messages.
 *
 * @param lvl The new debug level.
 */
void log_setlevel(int lvl);

/** Reset the wallclock of debug messages. */
void log_reset();

/** Logs variadic messages to stdout.
 *
 * @param lvl The log level
 * @param fmt The format string (printf alike)
 */
void log_print(const char *lvl, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

/** Logs variadic messages to stdout.
 *
 * @param lvl The log level
 * @param fmt The format string (printf alike)
 * @param va The variadic argument list (see stdarg.h)
 */	
void log_vprint(const char *lvl, const char *fmt, va_list va);

/** Printf alike debug message with level. */
void debug(int lvl, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

/** Print a horizontal line. */
void line();

/** Printf alike info message. */
void info(const char *fmt, ...)
	__attribute__ ((format(printf, 1, 2)));

/** Printf alike warning message. */
void warn(const char *fmt, ...)
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

#endif /* _LOG_H_ */

