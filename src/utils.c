/**
 * Some helper functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "utils.h"

static const char *log_prefix[] = {
	"Debug",
	"Info",
	"Warning",
	"Error",
	"Fatal"
};

void print(enum log_level lvl, const char *fmt, ...)
{
	struct timespec ts;

	va_list ap;
	va_start(ap, fmt);

	clock_gettime(CLOCK_REALTIME, &ts);

	printf("%14.3f [%-7s] ", ts.tv_sec + ts.tv_nsec / 1e9, log_prefix[lvl]);
	vprintf(fmt, ap);
	printf("\n");

	va_end(ap);

	if (lvl >= FATAL)
		exit(EXIT_FAILURE);
}
