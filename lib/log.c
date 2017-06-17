/** Logging and debugging routines
 *
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

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "config.h"
#include "log.h"
#include "utils.h"
#include "timing.h"

#ifdef ENABLE_OPAL_ASYNC
/* Define RTLAB before including OpalPrint.h for messages to be sent
 * to the OpalDisplay. Otherwise stdout will be used. */
  #define RTLAB
  #include "OpalPrint.h"
#endif

/** The global log instance. */
struct log *global_log;
struct log default_log = {
	.level = V,
	.facilities = LOG_ALL,
	.file = NULL,
	.path = NULL,
	.epoch = { -1 , -1 }
};

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
	"tc",		/* LOG_TC */
	"if",		/* LOG_IF */
	"advio",	/* LOG_ADVIO */

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
	global_log = l;

	l->level = level;
	l->facilities = facilitites;
	l->file = stderr;
	l->path = NULL;

	l->state = STATE_INITIALIZED;

	return 0;
}

int log_start(struct log *l)
{
	l->epoch = time_now();

	l->file = l->path ? fopen(l->path, "a+") : stderr;
	if (!l->file) {
		l->file = stderr;
		error("Failed to open log file '%s'", l->path);
	}

	l->state = STATE_STARTED;

	debug(LOG_LOG | 5, "Log sub-system started: level=%d, faciltities=%#lx, path=%s", l->level, l->facilities, l->path);

	return 0;
}

int log_stop(struct log *l)
{
	if (l->state == STATE_STARTED) {
		if (l->file != stderr && l->file != stdout)
			fclose(l->file);
	}

	l->state = STATE_STOPPED;

	return 0;
}

int log_destroy(struct log *l)
{
	default_log.epoch = l->epoch;

	global_log = NULL;

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
	fprintf(l->file ? l->file : stderr, "\33[2K\r%s\n", buf);
	free(buf);
}

void line()
{
	char buf[LOG_WIDTH];
	memset(buf, 0x71, sizeof(buf));

	log_print(global_log, "", "\b" ACS("%.*s"), LOG_WIDTH, buf);
}

void debug(long class, const char *fmt, ...)
{
	va_list ap;

	struct log *l = global_log ? global_log : &default_log;

	int lvl = class &  0xFF;
	int fac = class & ~0xFF;

	if (((fac == 0) || (fac & l->facilities)) && (lvl <= l->level)) {
		va_start(ap, fmt);
		log_vprint(l, LOG_LVL_DEBUG, fmt, ap);
		va_end(ap);
	}
}

void info(const char *fmt, ...)
{
	va_list ap;

	struct log *l = global_log ? global_log : &default_log;

	va_start(ap, fmt);
	log_vprint(l, LOG_LVL_INFO, fmt, ap);
	va_end(ap);
}

void warn(const char *fmt, ...)
{
	va_list ap;

	struct log *l = global_log ? global_log : &default_log;

	va_start(ap, fmt);
	log_vprint(l, LOG_LVL_WARN, fmt, ap);
	va_end(ap);
}

void stats(const char *fmt, ...)
{
	va_list ap;

	struct log *l = global_log ? global_log : &default_log;

	va_start(ap, fmt);
	log_vprint(l, LOG_LVL_STATS, fmt, ap);
	va_end(ap);
}

void error(const char *fmt, ...)
{
	va_list ap;

	struct log *l = global_log ? global_log : &default_log;

	va_start(ap, fmt);
	log_vprint(l, LOG_LVL_ERROR, fmt, ap);
	va_end(ap);

	die();
}

void serror(const char *fmt, ...)
{
	va_list ap;
	char *buf = NULL;

	struct log *l = global_log ? global_log : &default_log;

	va_start(ap, fmt);
	vstrcatf(&buf, fmt, ap);
	va_end(ap);

	log_print(l, LOG_LVL_ERROR, "%s: %m (%u)", buf, errno);

	free(buf);
	die();
}
