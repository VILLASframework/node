/** Logging and debugging routines
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */
 
 #ifndef _LOG_H_
#define _LOG_H_

#ifdef __GNUC__
 #define INDENT		int __attribute__ ((__cleanup__(log_outdent), unused)) _old_indent = log_indent(1);
#else
 #define INDENT		;
#endif

/** Global debug level used by the debug() macro.
 * It defaults to V (defined by the Makefile) and can be
 * overwritten by the 'debug' setting in the config file.
 */
extern int _debug;

/** The log level which is passed as first argument to print() */
enum log_level {
	DEBUG,
	INFO,
	WARN,
	ERROR
};

int log_indent(int levels);

void log_outdent(int *);

/** Reset the wallclock of debugging outputs */
void log_reset();

/** Logs variadic messages to stdout.
 *
 * @param lvl The log level
 * @param fmt The format string (printf alike)
 */
void log_print(enum log_level lvl, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

/** Printf alike debug message with level. */
#define debug(lvl, msg, ...) do if (lvl <= _debug) log_print(DEBUG, msg, ##__VA_ARGS__); while (0)

/** Printf alike info message. */
#define info(msg, ...) do log_print(INFO, msg, ##__VA_ARGS__); while (0)

/** Printf alike warning message. */
#define warn(msg, ...) do log_print(WARN, msg, ##__VA_ARGS__); while (0)

/** Print error and exit. */
#define error(msg, ...) do { \
		log_print(ERROR, msg, ##__VA_ARGS__); \
		die(); \
	} while (0)

/** Print error and strerror(errno). */
#define serror(msg, ...) do { \
		log_print(ERROR, msg ": %s", ##__VA_ARGS__, strerror(errno)); \
		die(); \
	} while (0)

/** Print configuration error and exit. */
#define cerror(c, msg, ...) do { \
		log_print(ERROR, msg " in %s:%u", ##__VA_ARGS__, \
			(config_setting_source_file(c)) ? \
			 config_setting_source_file(c) : "(stdio)", \
			config_setting_source_line(c)); \
		die(); \
	} while (0)

#endif /* _LOG_H_ */

