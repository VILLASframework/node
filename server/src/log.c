/** Logging and debugging routines
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 */

 #include <stdio.h>
#include <time.h>

#include "log.h"
#include "utils.h"

int _debug = V;

static struct timespec epoch;

#ifdef __GNUC__
static __thread int indent = 0;

/** Get thread-specific pointer to indent level */
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

void log_reset()
{
	clock_gettime(CLOCK_REALTIME, &epoch);
}

void log_print(enum log_level lvl, const char *fmt, ...)
{
	struct timespec ts;
	char buf[512] = "";

	va_list ap;

	/* Timestamp */
	clock_gettime(CLOCK_REALTIME, &ts);
	strap(buf, sizeof(buf), "%8.3f ", timespec_delta(&epoch, &ts));

	/* Severity */
	switch (lvl) {
		case DEBUG: strap(buf, sizeof(buf), BLD("%-5s "), GRY("Debug")); break;
		case INFO:  strap(buf, sizeof(buf), BLD("%-5s "),     "     " ); break;
		case WARN:  strap(buf, sizeof(buf), BLD("%-5s "), YEL(" Warn")); break;
		case ERROR: strap(buf, sizeof(buf), BLD("%-5s "), RED("Error")); break;
	}

	/* Indention */
#ifdef __GNUC__
	for (int i = 0; i < indent; i++)
		strap(buf, sizeof(buf), GFX("\x78") " ");
	strap(buf, sizeof(buf), GFX("\x74") " ");
#endif

	/* Format String */
	va_start(ap, fmt);
	vstrap(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	/* Output */
#ifdef ENABLE_OPAL_ASYNC
	OpalPrint("S2SS: %s\n", buf);
#endif
	fprintf(stderr, "\r%s\n", buf);
}
