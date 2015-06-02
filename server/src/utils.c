/** Some helper functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
#include <fstab.h>

#ifdef ENABLE_OPAL_ASYNC
#define RTLAB
#include <OpalPrint.h>
#endif

#include "config.h"
#include "cfg.h"
#include "utils.h"

pthread_t _mtid;

double box_muller(float m, float s)
{
	double x1, x2, y1;
	static double y2;
	static int use_last = 0;

	if (use_last) {		/* use value from previous call */
		y1 = y2;
		use_last = 0;
	}
	else {
		double w;
		do {
			x1 = 2.0 * randf() - 1.0;
			x2 = 2.0 * randf() - 1.0;
			w = x1*x1 + x2*x2;
		} while (w >= 1.0);

		w = sqrt(-2.0 * log(w) / w);
		y1 = x1 * w;
		y2 = x2 * w;
		use_last = 1;
	}

	return m + y1 * s;
}

double randf()
{
	return (double) random() / RAND_MAX;
}

void die()
{
	if (pthread_equal(_mtid, pthread_self()))
		exit(EXIT_FAILURE);
	else
		pthread_kill(_mtid, SIGINT);
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

uint64_t timerfd_wait(int fd)
{
	uint64_t runs;
	
	return read(fd, &runs, sizeof(runs)) < 0 ? 0 : runs;
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
	va_list ap;
	char buf[1024];
	
	va_start(ap, cmd);
	vsnprintf(buf, sizeof(buf), cmd, ap);
	va_end(ap);
	
	strap(buf, sizeof(buf), " 2>&1", sizeof(buf)); /* redirect stderr to stdout */
		
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
