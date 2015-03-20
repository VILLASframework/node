/** Some helper functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <math.h>

#ifdef ENABLE_OPAL_ASYNC
#include <OpalPrint.h>
#endif

#include "config.h"
#include "cfg.h"
#include "utils.h"

/* This global variable contains the debug level for debug() and assert() macros */
int _debug = V;
int _indent = 0;

struct timespec epoch;

void outdent(int *old)
{
	_indent = *old;
}

void epoch_reset()
{
	clock_gettime(CLOCK_REALTIME, &epoch);
}

int strap(char *dest, size_t size, const char *fmt,  ...)
{
	int ret;
	
	va_list ap;
	va_start(ap, fmt);
	ret = vstrap(dest, size, fmt, ap);
	va_end(ap);
	
	return ret;
}

int vstrap(char *dest, size_t size, const char *fmt, va_list ap)
{
	int len = strlen(dest);

	return vsnprintf(dest + len, size - len, fmt, ap);
}

void print(enum log_level lvl, const char *fmt, ...)
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
	for (int i = 0; i < _indent-1; i++)
		strap(buf, sizeof(buf), GFX("\x78") " ");
	strap(buf, sizeof(buf), GFX("\x74") " ");

	/* Format String */
	va_start(ap, fmt);
	vstrap(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	
	/* Output */
#ifdef ENABLE_OPAL_ASYNC
	OpalPrint("%s\n", buf);
#else
	fprintf(stderr, "%s\n", buf);
#endif
}

cpu_set_t to_cpu_set(int set)
{
	cpu_set_t cset;

	CPU_ZERO(&cset);

	for (int i = 0; i < sizeof(set) * 8; i++) {
		if (set & (1L << i))
			CPU_SET(i, &cset);
	}

	return cset;
}

void * alloc(size_t bytes)
{
	void *p = malloc(bytes);
	if (!p)
		error("Failed to allocate memory");

	memset(p, 0, bytes);
	
	return p;
}

double timespec_delta(struct timespec *start, struct timespec *end)
{
	double sec  = end->tv_sec - start->tv_sec;
	double nsec = end->tv_nsec - start->tv_nsec;

	if (nsec < 0) {
		sec  -= 1;
		nsec += 1e9;
	}

	return sec + nsec * 1e-9;
}

struct timespec timespec_rate(double rate)
{
	struct timespec ts;

	ts.tv_sec  = 1 / rate;
	ts.tv_nsec = 1.0e9 * (1 / rate - ts.tv_sec);

	return ts;
}

/** @todo: Proper way: create additional pipe for stderr in child process */
int system2(const char *cmd, ...)
{
	char buf[1024];
	
	va_list ap;
	va_start(ap, cmd);

		vsnprintf(buf, sizeof(buf), cmd, ap);
		strncat(buf, " 2>&1", sizeof(buf));
	
	va_end(ap);
	
	debug(1, "System: %s", buf);

	FILE *f = popen(buf, "r");
	if (f == NULL)
		serror("Failed to execute: '%s'", cmd);

	while (!feof(f) && fgets(buf, sizeof(buf), f) != NULL) { INDENT
		strtok(buf, "\n"); /* strip trailing newline */
		info(buf);
	}

	return pclose(f);
}
