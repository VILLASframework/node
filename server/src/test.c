/** Some basic tests.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "cfg.h"
#include "msg.h"
#include "node.h"
#include "utils.h"
#include "hist.h"
#include "timing.h"

/** Linked list of nodes */
struct list nodes;
/** The global configuration */
struct settings settings;

static struct node *node;

/* Test options */
static int running = 1;

/** Amount of messages which should be sent (default: -1 for unlimited) */
static int count =  -1;

/** File descriptor for Matlab results.
 * This allows you to write Matlab results in a seperate log file:
 *
 *    ./test etc/example.conf rtt -f 3 3>> measurement_results.m
 */
static int fd = STDOUT_FILENO;

/** Lowest value in histogram. */
static double low = 0;
/** Highest value in histogram. */
static double high = 2e-4;
/** Histogram resolution. */
static double res = 1e-5;

#define CLOCK_ID	CLOCK_MONOTONIC

/* Prototypes */
void test_rtt();

void quit(int sig, siginfo_t *si, void *ptr)
{
	running = 0;
}

int main(int argc, char *argv[])
{
	config_t config;
	
	_mtid = pthread_self();

	if (argc < 4) {
		printf("Usage: %s CONFIG TEST NODE [ARGS]\n", argv[0]);
		printf("  CONFIG  path to a configuration file\n");
		printf("  TEST    the name of the test to execute: 'rtt'\n");
		printf("  NODE    name of the node which shoud be used\n\n");

		printf("Simulator2Simulator Server %s (built on %s %s)\n",
			BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
		printf(" copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC\n");
		printf(" Steffen Vogel <StVogel@eonerc.rwth-aachen.de>\n");

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
	config_parse(argv[1], &config, &settings, &nodes, NULL);
	
	node = node_lookup_name(argv[3], &nodes);
	if (!node)
		error("There's no node with the name '%s'", argv[3]);

	node->refcnt++;
	node->vt->refcnt++;

	node_start(node);
	node_start_defer(node);
	
	/* Parse Arguments */
	char c, *endptr;
	while ((c = getopt (argc-3, argv+3, "l:h:r:f:c:")) != -1) {
		switch (c) {
			case 'c':
				count = strtoul(optarg, &endptr, 10);
				goto check;
			case 'f':
				fd = strtoul(optarg, &endptr, 10);
				goto check;
			case 'l':
				low = strtod(optarg, &endptr);
				goto check;
			case 'h':
				high = strtod(optarg, &endptr);
				goto check;
			case 'r':
				res = strtod(optarg, &endptr);
				goto check;
			case '?':
				if (optopt == 'c')
					error("Option -%c requires an argument.", optopt);
				else if (isprint(optopt))
					error("Unknown option '-%c'.", optopt);
				else
					error("Unknown option character '\\x%x'.", optopt);
				exit(EXIT_FAILURE);
			default:
				abort();
		}
		
		continue;
check:
		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);
	}

	if (!strcmp(argv[2], "rtt"))
		test_rtt();
	else
		error("Unknown test: '%s'", argv[2]);

	node_stop(node);
	config_destroy(&config);

	return 0;
}

void test_rtt() {
	struct msg m = MSG_INIT(sizeof(struct timespec) / sizeof(float));
	
	struct timespec ts;
	struct timespec *ts1 = (struct timespec *) &m.data;
	struct timespec *ts2 = alloc(sizeof(struct timespec));

	double rtt;
	struct hist hist;
	hist_create(&hist, low, high, res);

	/* Print header */
	fprintf(stdout, "%17s%5s%10s%10s%10s%10s%10s\n", "timestamp", "seq", "rtt", "min", "max", "mean", "stddev");

	while (running && (count < 0 || count--)) {
		clock_gettime(CLOCK_ID, ts1);
		node_write_single(node, &m); /* Ping */
		node_read_single(node, &m);  /* Pong */
		clock_gettime(CLOCK_ID, ts2);

		rtt = time_delta(ts1, ts2);

		if (rtt < 0)
			warn("Negative RTT: %f", rtt);
	
		hist_put(&hist, rtt);

		clock_gettime(CLOCK_REALTIME, &ts);
		time_fprint(stdout, &ts);

		m.sequence++;

		fprintf(stdout, "%5u%10.3f%10.3f%10.3f%10.3f%10.3f\n", m.sequence,
			1e3 * rtt, 1e3 * hist.lowest, 1e3 * hist.highest,
			1e3 * hist_mean(&hist), 1e3 * hist_stddev(&hist));
	}

	free(ts2);

	hist_print(&hist);
	
	struct stat st;
	if (!fstat(fd, &st)) {
		FILE *f = fdopen(fd, "w");
		hist_matlab(&hist, f);
	}
	else
		error("Invalid file descriptor: %u", fd);
	
	hist_destroy(&hist);
}
