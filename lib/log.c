/** Logging and debugging routines
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
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

static struct log *log;

/** List of debug facilities as strings */
static const char *facilities_strs[] = {
	"pool",		/* LOG_POOL */
	"queue",	/* LOG_QUEUE */
	"config",	/* LOG_CONFIG */
	"hook",		/* LOG_HOOK */
	"path",		/* LOG_PATH */
	"mem",		/* LOG_MEM */
	"web",		/* LOG_WEB */
	"api",		/* LOG_API */
	
	/* Node-types */	
	"socket",	/* LOG_SOCKET */
	"file",		/* LOG_FILE */
	"fpga",		/* LOG_FPGA */
	"ngsi",		/* LOG_NGSI */
	"websocket",	/* LOG_WEBSOCKET */
	"opal"		/* LOG_OPAL */
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

int log_set_facility_expression(struct log *l, const char *expression)
{
	char *copy, *facility_str;
	
	enum {
		NORMAL,
		NEGATE
	} mode;
	
	if (strlen(expression) <= 0)
		return -1;

	if (expression[0] == '!') {
		mode = NEGATE;
		l->facilities = ~0xFF;
	}
	else {
		mode = NORMAL;
		l->facilities = 0;
	}

	copy = strdup(expression);
	facility_str = strtok(copy, ",");

	while (facility_str != NULL) {
		for (int i = 0; i < ARRAY_LEN(facilities_strs); i++) {
			if (strcmp(facilities_strs[i], facility_str)) {
				switch (mode) {
					case NORMAL: l->facilities |=  (1 << (i+8));
					case NEGATE: l->facilities &= ~(1 << (i+8));
				}
			}
		}
		
		facility_str = strtok(NULL, ",");
	}
	
	free(copy);

	return l->facilities;
}

int log_init(struct log *l)
{
	l->epoch = time_now();
	l->level = V;
	l->facilities = LOG_ALL;

	debug(LOG_LOG | 10, "Log sub-system intialized");
	
	/* Register this log instance globally */
	log = l;

	return 0;
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

int log_parse(struct log *l, config_setting_t *cfg)
{
	const char *facilities;

	config_setting_lookup_int(cfg, "level", &l->level);

	if (config_setting_lookup_string(cfg, "facilties", &facilities))
		log_set_facility_expression(l, facilities);

	return 0;
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

	if (((fac == 0) || (fac & log->facilities)) && (lvl <= log->level)) {
		va_start(ap, fmt);
		log_vprint(log, LOG_LVL_DEBUG, fmt, ap);
		va_end(ap);
	}
}

void info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(log, LOG_LVL_INFO, fmt, ap);
	va_end(ap);
}

void warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(log, LOG_LVL_WARN, fmt, ap);
	va_end(ap);
}

void stats(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(log, LOG_LVL_STATS, fmt, ap);
	va_end(ap);
}

void error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprint(log, LOG_LVL_ERROR, fmt, ap);
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

	log_print(log, LOG_LVL_ERROR, "%s: %m (%u)", buf, errno);
	
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

	log_print(log, LOG_LVL_ERROR, "%s in %s:%u", buf,
		config_setting_source_file(cfg)
		   ? config_setting_source_file(cfg)
		   : "(stdio)",
		config_setting_source_line(cfg));

	free(buf);
	die();
}
