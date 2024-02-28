/** Logging and debugging routines
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

#include <villas/config.h>
#include <villas/log.h>
#include <villas/utils.h>

#ifdef ENABLE_OPAL_ASYNC
/* Define RTLAB before including OpalPrint.h for messages to be sent
 * to the OpalDisplay. Otherwise stdout will be used. */
  #define RTLAB
  #include "OpalPrint.h"
#endif

struct log *global_log;
struct log default_log;

/* We register a default log instance */
__attribute__((constructor))
void register_default_log()
{
	int ret;
	static struct log default_log;

	ret = log_init(&default_log, V, LOG_ALL);
	if (ret)
		error("Failed to initalize log");

	ret = log_start(&default_log);
	if (ret)
		error("Failed to start log");
}

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

int log_indent(int levels)
{
	int old = indent;
	indent += levels;
	return old;
}

int log_noindent()
{
	int old = indent;
	indent = 0;
	return old;
}

void log_outdent(int *old)
{
	indent = *old;
}
#endif

static void log_resize(int signal, siginfo_t *sinfo, void *ctx)
{
	int ret;

	ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &global_log->window);
	if (ret)
		return;

	global_log->width = global_log->window.ws_col - 25;
	if (global_log->prefix)
		global_log->width -= strlenp(global_log->prefix);

	debug(LOG_LOG | 15, "New terminal size: %dx%x", global_log->window.ws_row, global_log->window.ws_col);
}

int log_init(struct log *l, int level, long facilitites)
{
	int ret;

	/* Register this log instance globally */
	global_log = l;

	l->level = level;
	l->syslog = 0;
	l->facilities = facilitites;
	l->file = stderr;
	l->path = NULL;

	l->prefix = getenv("VILLAS_LOG_PREFIX");

	/* Register signal handler which is called whenever the
	 * terminal size changes. */
	if (l->file == stderr) {
		struct sigaction sa_resize = {
			.sa_flags = SA_SIGINFO,
			.sa_sigaction = log_resize
		};

		sigemptyset(&sa_resize.sa_mask);

		ret = sigaction(SIGWINCH, &sa_resize, NULL);
		if (ret)
			return ret;

		/* Try to get initial window size */
		ioctl(STDERR_FILENO, TIOCGWINSZ, &global_log->window);
	}
	else {
		l->window.ws_col = LOG_WIDTH;
		l->window.ws_row = LOG_HEIGHT;
	}

	l->width = l->window.ws_col - 25;
	if (l->prefix)
		l->width -= strlenp(l->prefix);

	l->state = STATE_INITIALIZED;

	return 0;
}

int log_start(struct log *l)
{
	if (l->path) {
		l->file = fopen(l->path, "a+");;
		if (!l->file) {
			l->file = stderr;
			error("Failed to open log file '%s'", l->path);
		}
	}
	else
		l->file = stderr;

	l->state = STATE_STARTED;

	if (l->syslog) {
		openlog(NULL, LOG_PID, LOG_DAEMON);
	}

	debug(LOG_LOG | 5, "Log sub-system started: level=%d, faciltities=%#lx, path=%s", l->level, l->facilities, l->path);

	return 0;
}

int log_stop(struct log *l)
{
	if (l->state != STATE_STARTED)
		return 0;

	if (l->file != stderr && l->file != stdout) {
		fclose(l->file);
	}

	if (l->syslog) {
		closelog();
	}

	l->state = STATE_STOPPED;

	return 0;
}

int log_destroy(struct log *l)
{
	default_log.epoch = l->epoch;

	global_log = &default_log;

	l->state = STATE_DESTROYED;

	return 0;
}

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
	char *buf = alloc(512);

	/* Optional prefix */
	if (l->prefix)
		strcatf(&buf, "%s", l->prefix);

	/* Indention */
#ifdef __GNUC__
	for (int i = 0; i < indent; i++)
		strcatf(&buf, "%s ", BOX_UD);

	strcatf(&buf, "%s ", BOX_UDR);
#endif

	/* Format String */
	vstrcatf(&buf, fmt, ap);

	/* Output */
#ifdef ENABLE_OPAL_ASYNC
	OpalPrint("VILLASfpga: %s\n", buf);
#endif
	fprintf(l->file ? l->file : stderr, "%s\n", buf);

	free(buf);
}
