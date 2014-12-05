/** Some basic tests.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "config.h"
#include "cfg.h"
#include "msg.h"
#include "node.h"
#include "utils.h"

static struct settings set;
static struct node *node;
extern struct node *nodes;

int running = 1;

#define CLOCK_ID	CLOCK_MONOTONIC_RAW

#define RTT_MIN		20
#define RTT_MAX		100
#define RTT_RESOLUTION  2
#define RTT_HIST	(int) ((RTT_MAX - RTT_MIN) / RTT_RESOLUTION)

void quit(int sig, siginfo_t *si, void *ptr)
{
	running = 0;
}

int main(int argc, char *argv[])
{
	config_t config;

	if (argc != 4) {
		printf("Usage: %s CONFIG NODE\n", argv[0]);
		printf("  CONFIG  path to a configuration file\n");
		printf("  NODE    name of the node which shoud be used\n\n");
		printf("Simulator2Simulator Server %s (built on %s %s)\n",
			BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
		printf(" Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		printf("   Steffen Vogel <stvogel@eonerc.rwth-aachen.de>\n");
		exit(EXIT_FAILURE);
	}

	/* Setup signals */
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT, &sa_quit, NULL);

	config_init(&config);
	config_parse(argv[1], &config, &set, &nodes, NULL);
	
	node = node_lookup_name(argv[2], nodes);
	if (!node)
		error("There's no node with the name '%s'", argv[2]);

	node_start(node);
	node_start_defer(node);

	if (!strcmp(argv[1], "rtt")) {
		struct msg m = MSG_INIT(sizeof(struct timespec) / sizeof(float));
		struct timespec *ts1 = (struct timespec *) &m.data;
		struct timespec *ts2 = malloc(sizeof(struct timespec));

		double rtt;
		double rtt_max = LLONG_MIN;
		double rtt_min = LLONG_MAX;
		double avg = 0;
		int bar;
		unsigned hist[RTT_HIST];


		memset(hist, 0, RTT_HIST * sizeof(unsigned));

#if 1		/* Print header */
		fprintf(stdout, "%17s", "timestamp");
#endif
		fprintf(stdout, "%5s%10s%10s%10s%10s\n", "seq", "rtt", "min", "max", "avg");

		while (running) {
			clock_gettime(CLOCK_ID, ts1);
			node_write(node, &m);
			node_read(node, &m);
			clock_gettime(CLOCK_ID, ts2);

			rtt = timespec_delta(ts1, ts2);

			if (rtt < 0) continue;
			if (rtt > rtt_max) rtt_max = rtt;
			if (rtt < rtt_min) rtt_min = rtt;

			avg += rtt;

			/* Update histogram */
			bar = (rtt * 1000 / RTT_RESOLUTION) - (RTT_MIN / RTT_RESOLUTION);
			if (bar < RTT_HIST)
				hist[bar]++;

#if 1
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			fprintf(stdout, "%17.6f", ts.tv_sec + ts.tv_nsec / 1e9);
#endif
			m.sequence++;

			fprintf(stdout, "%5u%10.3f%10.3f%10.3f%10.3f\n", m.sequence,
				1e3 * rtt, 1e3 * rtt_min, 1e3 * rtt_max, 1e3 * avg / m.sequence);
		}

		free(ts2);

		hist_plot(hist, RTT_HIST);
		hist_dump(hist, RTT_HIST);
	}

	node_stop(node);

	return 0;
}
