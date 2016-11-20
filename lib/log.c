/** Logging and debugging routines
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
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
static unsigned level = V;

/** Debug facilities used by the debug() macro. */
static unsigned facilities = ~0;

/** A global clock used to prefix the log messages. */
static struct timespec epoch;

/** Handle to log file */
static FILE *file;

static const char *facility_names[] = {
	"pool",		// DBG_POOL
	"queue",	// DBG_QUEUE
	"config", 	// DBG_CONFIG
	"hook",		// DBG_HOOK
	"path",		// DBG_PATH
	"mem",		// DBG_MEM
	
	/* Node-types */
	"socket",	// DBG_SOCKET
	"websocket",	// DBG_WEBSOCKET
	"file",		// DBG_FILE
	"fpga",		// DBG_FPGA
	"ngsi",		// DBG_NGSI
	"opal",		// DBG_OPAL
	NULL
};

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

void log_init(int lvl, int fac, const char *path)
{
	char *fac_str = NULL;

	/* Initialize global variables */
	epoch = time_now();
	level = lvl;
	facilities = fac;
	
	if (path)
		file = fopen(path, "a");

	debug(10, "Started logging: level = %u, faciltities = %s", level, fac_str);
}

int log_lookup_facility(const char *facility_name)
{
	for (int i = 0; facility_names[i]; i++) {
		if (!strcmp(facility_name, facility_names[i]))
			return 1 << i;
	}
	
	return 0;
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
	struct timespec ts = time_now();
	char *buf = alloc(512);
	
	/* Timestamp */
	strcatf(&buf, "%10.3f ", time_delta(&epoch, &ts));

	/* Severity */
	strcatf(&buf, "%5s ", lvl);

	/* Indention */
#ifdef __GNUC__
	for (int i = 0; i < indent; i++)
		strcatf(&buf, ACS_VERTICAL " ");

	strcatf(&buf, ACS_VERTRIGHT " ");
#endif

	/* Format String */
	vstrcatf(&buf, fmt, ap);

	/* Output */
#ifdef ENABLE_OPAL_ASYNC
	OpalPrint("VILLASnode: %s\n", buf);
#endif
	fprintf(stderr, "\r%s\n", buf);
	
	if (file)
		fprintf(file, "%s\n", buf);
	
	free(buf);
}

void line()
{
	char buf[LOG_WIDTH];
	memset(buf, 0x71, sizeof(buf));

	log_print("", "\b" ACS("%.*s"), LOG_WIDTH, buf);
}

void debug(int class, const char *fmt, ...)
{
	va_list ap;
	
	int lvl = class &  0xFF;
	int fac = class & ~0xFF;

	if (((fac == 0) || (fac & facilities)) && (lvl <= level)) {
		va_start(ap, fmt);
		log_vprint(LOG_LVL_DEBUG, fmt, ap);
		va_end(ap);
	}
}

void info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(LOG_LVL_INFO, fmt, ap);
	va_end(ap);
}

void warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(LOG_LVL_WARN, fmt, ap);
	va_end(ap);
}

void stats(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(LOG_LVL_STATS, fmt, ap);
	va_end(ap);
}

void error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(LOG_LVL_ERROR, fmt, ap);
	va_end(ap);

	die();
}

void serror(const char *fmt, ...)
{
	va_list ap;
	char *buf = NULL;

	va_start(ap, fmt);
	vstrcatf(&buf, fmt, ap);
	va_end(ap);

	log_print(LOG_LVL_ERROR, "%s: %m (%u)", buf, errno);
	
	free(buf);
	die();
}

void cerror(config_setting_t *cfg, const char *fmt, ...)
{
	va_list ap;
	char *buf = NULL;

	va_start(ap, fmt);
	vstrcatf(&buf, fmt, ap);
	va_end(ap);

	log_print(LOG_LVL_ERROR, "%s in %s:%u", buf,
		config_setting_source_file(cfg)
		   ? config_setting_source_file(cfg)
		   : "(stdio)",
		config_setting_source_line(cfg));

	free(buf);
	die();
}
