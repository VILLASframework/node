/** Logging and debugging routines
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdio.h>
#include <stdbool.h>
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

/** The global log instance. */
static struct log *log;

/** List of debug facilities as strings */
static const char *facilities_strs[] = {
	"pool",		/* LOG_POOL */
	"queue",	/* LOG_QUEUE */
	"config",	/* LOG_CONFIG */
	"hook",		/* LOG_HOOK */
	"path",		/* LOG_PATH */
	"node",		/* LOG_NODE */
	"mem",		/* LOG_MEM */
	"web",		/* LOG_WEB */
	"api",		/* LOG_API */
	"log",		/* LOG_LOG */
	"vfio",		/* LOG_VFIO */
	"pci",		/* LOG_PCI */
	"xil",		/* LOG_XIL */
	
	/* Node-types */	
	"socket",	/* LOG_SOCKET */
	"file",		/* LOG_FILE */
	"fpga",		/* LOG_FPGA */
	"ngsi",		/* LOG_NGSI */
	"websocket",	/* LOG_WEBSOCKET */
	"opal",		/* LOG_OPAL */
};

#ifdef __GNUC__
/** The current log indention level (per thread!). */
static __thread int indent = 0;

int log_init(struct log *l, int level, long facilitites)
{
	/* Register this log instance globally */
	log = l;

	l->level = level;
	l->facilities = facilitites;
	
	debug(LOG_LOG | 5, "Log sub-system intialized: level=%d, faciltities=%#lx", level, facilitites);
	
	l->state = STATE_INITIALIZED;
	
	return 0;
}

int log_parse(struct log *l, config_setting_t *cfg)
{
	const char *facilities;
	
	if (!config_setting_is_group(cfg))
		cerror(cfg, "Setting 'log' must be a group.");

	config_setting_lookup_int(cfg, "level", &l->level);

	if (config_setting_lookup_string(cfg, "facilities", &facilities))
		log_set_facility_expression(l, facilities);
	
	l->state = STATE_PARSED;

	return 0;
}

int log_start(struct log *l)
{
	l->epoch = time_now();
	
	l->state = STATE_STARTED;

	return 0;
}

int log_stop(struct log *l)
{
	if (l->state != STATE_STARTED)
		return -1;
	
	l->state = STATE_STOPPED;
	
	return 0;
}

int log_destroy(struct log *l)
{
	if (l->state == STATE_STARTED)
		return -1;
	
	l->state = STATE_DESTROYED;
	
	return 0;
}

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

int log_set_facility_expression(struct log *l, const char *expression)
{
	bool negate;
	char *copy, *token;
	long mask = 0, facilities = 0;
	
	copy = strdup(expression);
	token = strtok(copy, ",");

	while (token != NULL) {
		if (token[0] == '!') {
			token++;
			negate = true;
		}
		else
			negate = false;

		/* Check for some classes */
		if      (!strcmp(token, "all"))
			mask = LOG_ALL;
		else if (!strcmp(token, "nodes"))
			mask = LOG_NODES;
		else if (!strcmp(token, "kernel"))
			mask = LOG_KERNEL;
		else {
			for (int ind = 0; ind < ARRAY_LEN(facilities_strs); ind++) {
				if (!strcmp(token, facilities_strs[ind])) {
					mask = (1 << (ind+8));
					goto found;
				}
			}
			
			error("Invalid log class '%s'", token);
		}

found:		if (negate)
			facilities &= ~mask;
		else
			facilities |= mask;

		token = strtok(NULL, ",");
	}
	
	l->facilities = facilities;
	
	free(copy);

	return l->facilities;
}

void log_print(struct log *l, const char *lvl, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(l, lvl, fmt, ap);
	va_end(ap);
}

void log_vprint(struct log *l, const char *lvl, const char *fmt, va_list ap)
{
	struct timespec ts = time_now();
	char *buf = alloc(512);
	
	/* Timestamp */
	strcatf(&buf, "%10.3f ", time_delta(&l->epoch, &ts));

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
	free(buf);
}

void line()
{
	char buf[LOG_WIDTH];
	memset(buf, 0x71, sizeof(buf));

	log_print(log, "", "\b" ACS("%.*s"), LOG_WIDTH, buf);
}

void debug(long class, const char *fmt, ...)
{
	va_list ap;
	
	int lvl = class &  0xFF;
	int fac = class & ~0xFF;
	
	assert(log);

	if (((fac == 0) || (fac & log->facilities)) && (lvl <= log->level)) {
		va_start(ap, fmt);
		log_vprint(log, LOG_LVL_DEBUG, fmt, ap);
		va_end(ap);
	}
}

void info(const char *fmt, ...)
{
	va_list ap;
	
	assert(log);

	va_start(ap, fmt);
	log_vprint(log, LOG_LVL_INFO, fmt, ap);
	va_end(ap);
}

void warn(const char *fmt, ...)
{
	va_list ap;
	
	assert(log);

	va_start(ap, fmt);
	log_vprint(log, LOG_LVL_WARN, fmt, ap);
	va_end(ap);
}

void stats(const char *fmt, ...)
{
	va_list ap;

	assert(log);

	va_start(ap, fmt);
	log_vprint(log, LOG_LVL_STATS, fmt, ap);
	va_end(ap);
}

void error(const char *fmt, ...)
{
	va_list ap;
	
	assert(log);

	va_start(ap, fmt);
	log_vprint(log, LOG_LVL_ERROR, fmt, ap);
	va_end(ap);

	die();
}

void serror(const char *fmt, ...)
{
	va_list ap;
	char *buf = NULL;

	assert(log);

	va_start(ap, fmt);
	vstrcatf(&buf, fmt, ap);
	va_end(ap);

	log_print(log, LOG_LVL_ERROR, "%s: %m (%u)", buf, errno);
	
	free(buf);
	die();
}

void cerror(config_setting_t *cfg, const char *fmt, ...)
{
	va_list ap;
	char *buf = NULL;

	assert(log);

	va_start(ap, fmt);
	vstrcatf(&buf, fmt, ap);
	va_end(ap);

	log_print(log, LOG_LVL_ERROR, "%s in %s:%u", buf,
		config_setting_source_file(cfg)
		   ? config_setting_source_file(cfg)
		   : "(stdio)",
		config_setting_source_line(cfg));

	free(buf);
	die();
}
