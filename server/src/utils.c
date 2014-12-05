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

void print(enum log_level lvl, const char *fmt, ...)
{
	struct timespec ts;

	va_list ap;
	va_start(ap, fmt);

	/* Timestamp */
	clock_gettime(CLOCK_REALTIME, &ts);
	fprintf(stderr, "%8.3f ", timespec_delta(&epoch, &ts));

	switch (lvl) {
		case DEBUG: fprintf(stderr, BLD("%-5s "), GRY("Debug")); break;
		case INFO:  fprintf(stderr, BLD("%-5s "),     "     " ); break;
		case WARN:  fprintf(stderr, BLD("%-5s "), YEL(" Warn")); break;
		case ERROR: fprintf(stderr, BLD("%-5s "), RED("Error")); break;
	}

	if (_indent) {
		for (int i = 0; i < _indent-1; i++)
			fprintf(stderr, GFX("\x78") " ");

int print_addr(struct sockaddr *sa, char *buf, size_t len)
{
	if (!sa)
		return -1;

	switch (sa->sa_family) {
		case AF_INET: {
			struct sockaddr_in *sin = (struct sockaddr_in *) sa;
			inet_ntop(sa->sa_family, sa, buf, len);
			snprintf(buf+strlen(buf), len-strlen(buf), ":%hu", ntohs(sin->sin_port))
			break;
		}

		case AF_PACKET: {
			struct sockaddr_ll *sll = (struct sockaddr_ll *) sa;
			char ifname[IF_NAMESIZE];

			snprintf("%#x:%#x:%#x:%#x:%#x:%#x:%hu (%s)",
				sll->sll_addr[0], sll->sll_addr[1], sll->sll_addr[2],
				sll->sll_addr[3], sll->sll_addr[4], sll->sll_addr[5],
				ntohs(sll->sll_protocol), if_indextoname(sll->sll_ifindex, ifname));
			break;
		}

		default:
			error("Unsupported address family");
		fprintf(stderr, GFX("\x74") " ");
	}
}

int parse_addr(const char *addr, struct sockaddr_in *sa, int flags)
{
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

	int ret = getaddrinfo(node, service, &hint, &result);
	if (!ret) {
		memcpy(sa, result->ai_addr, result->ai_addrlen);
		freeaddrinfo(result);
	}

	free(tmp);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");

	return ret;
	va_end(ap);
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
		if (hist[i] == 0)
			info("%2u | " GRN("%5u") " | "           ,  i, hist[i]);
		else if (hist[i] == hist[max])
			info("%2u | " RED("%5u") " | " BLD("%.*s"), i, hist[i], bar, buf);
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
