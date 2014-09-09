/** Some helper functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <math.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"
#include "cfg.h"
#include "utils.h"

/* This global variable contains the debug level for debug() and assert() macros */
int debug = V;

void print(enum log_level lvl, const char *fmt, ...)
{
	struct timespec ts;

	va_list ap;
	va_start(ap, fmt);

	clock_gettime(CLOCK_REALTIME, &ts);

	/* Timestamp */
	printf("%15.4f", ts.tv_sec + ts.tv_nsec / 1e9);

	switch (lvl) {
		case DEBUG: printf(" [" BLU("Debug") "] "); break;
		case INFO:  printf(" [" WHT("Info ") "] "); break;
		case WARN:  printf(" [" YEL("Warn ") "] "); break;
		case ERROR: printf(" [" RED("Error") "] "); break;
	}

	vprintf(fmt, ap);
	printf("\n");

	va_end(ap);
}

int resolve_addr(const char *addr, struct sockaddr_in *sa, int flags)
{
	int ret;

	/* Split string */
	char *tmp = strdup(addr);
	char *node = strtok(tmp, ":");
	char *service = strtok(NULL, ":");

	if (node && !strcmp(node, "*"))
		node = NULL;

	if (service && !strcmp(service, "*"))
		service = NULL;

	/* Get IP */
	struct addrinfo *result;
	struct addrinfo hint = {
		.ai_flags = flags,
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = 0
	};

	ret = getaddrinfo(node, service, &hint, &result);
	if (ret)
		error("Failed to lookup address '%s': %s", addr, gai_strerror(ret));

	memcpy(sa, result->ai_addr, result->ai_addrlen);

	free(tmp);
	freeaddrinfo(result);

	return 0;
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

void hist_plot(unsigned *hist, int length)
{
	char buf[HIST_HEIGHT + 32];
	int bar;
	int max = 0;

	/* Get max, first & last */
	for (int i = 0; i < length; i++) {
		if (hist[i] > hist[max])
			max = i;
	}


	/* Print header */
	info("%2s | %5s | %s", "Id", "Value", "Histogram Plot:");

	/* Print plot */
	memset(buf, '#', sizeof(buf));
	for (int i = 0; i < length; i++) {
		bar = HIST_HEIGHT * (float) hist[i] / hist[max];
		if      (hist[i] == hist[max])
			info("%2u | " RED("%5u") " | " BLD("%.*s"), i, hist[i], bar, buf);
		else if (hist[i] == 0)
			info("%2u | " GRN("%5u") " | "           ,  i, hist[i]);
		else
			info("%2u | "     "%5u"  " | "     "%.*s",  i, hist[i], bar, buf);
	}
}

void hist_dump(unsigned *hist, int length)
{
	char tok[16];
	char buf[length * sizeof(tok)];
	memset(buf, 0, sizeof(buf));

	/* Print in Matlab vector format */
	for (int i = 0; i < length; i++) {
		snprintf(tok, sizeof(tok), "%u ", hist[i]);
		strncat(buf, tok, sizeof(buf)-strlen(buf));
	}

	info("Matlab: hist = [ %s]", buf);
}
