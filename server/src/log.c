/** Logging and debugging routines
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "log.h"
#include "utils.h"
#include "config.h"
#include "timing.h"

#ifdef ENABLE_OPAL_ASYNC
/* Define RTLAB before including OpalPrint.h for messages to be sent
 * to the OpalDisplay. Otherwise stdout will be used. */
 #define RTLAB
 #include "OpalPrint.h"
#endif

/** Debug level used by the debug() macro.
 * It defaults to V (defined by the Makefile) and can be
 * overwritten by the 'debug' setting in the configuration file. */
static int level = V;

/** A global clock used to prefix the log messages. */
static struct timespec epoch;

#ifdef __GNUC__
/** The current log indention level (per thread!). */
static __thread int indent = 0;

int log_indent(int levels)
{
	int old = indent;
	indent += levels;
	return old;
}

void log_outdent(int *old)
{
	indent = *old;
}
#endif

void log_setlevel(int lvl)
{
	level = lvl;
	debug(10, "Switched to debug level %u", level);
}

void log_reset()
{
	clock_gettime(CLOCK_REALTIME, &epoch);
	debug(10, "Debug clock resetted");
}

void log_print(const char *lvl, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_vprint(lvl, fmt, ap);
	va_end(ap);
}

void log_vprint(const char *lvl, const char *fmt, va_list ap)
{
	struct timespec ts;
	char buf[512] = "";

	/* Timestamp */
	clock_gettime(CLOCK_REALTIME, &ts);
	strap(buf, sizeof(buf), "%10.3f ", time_delta(&epoch, &ts));

	/* Severity */
	strap(buf, sizeof(buf), BLD("%5s "), lvl);
	
	/* Indention */
#ifdef __GNUC__
	for (int i = 0; i < indent; i++)
		strap(buf, sizeof(buf), ACS_VERTICAL " ");
	strap(buf, sizeof(buf), ACS_VERTRIGHT " ");
#endif

	/* Format String */
	vstrap(buf, sizeof(buf), fmt, ap);
	
	/* Output */
#ifdef ENABLE_OPAL_ASYNC
	OpalPrint("S2SS: %s\n", buf);
#endif
	fprintf(stderr, "\r%s\n", buf);
}

void line()
{
	char buf[LOG_WIDTH];
	memset(buf, 0x71, sizeof(buf));
	
	log_print("", "\b" ACS("%.*s"), LOG_WIDTH, buf);
}

void debug(int lvl, const char *fmt, ...)
{
	va_list ap;

	if (lvl <= level) {
		va_start(ap, fmt);
		log_vprint(DEBUG, fmt, ap);
		va_end(ap);
	}
}

void info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(INFO, fmt, ap);
	va_end(ap);
}

void warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(WARN, fmt, ap);
	va_end(ap);
}
	
void error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(ERROR, fmt, ap);
	va_end(ap);

	die();
}

void serror(const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	log_print(ERROR, "%s: %m (%u)", buf, errno);
	die();
}

void cerror(config_setting_t *cfg, const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	log_print(ERROR, "%s in %s:%u", buf,
		config_setting_source_file(cfg)
		   ? config_setting_source_file(cfg)
		   : "(stdio)", 
		config_setting_source_line(cfg)); 
	die();
}
