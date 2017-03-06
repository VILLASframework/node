/** Some basic tests.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#include <villas/cfg.h>
#include <villas/node.h>
#include <villas/utils.h>
#include <villas/hist.h>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/kernel/rt.h>

#include "config.h"

struct cfg cfg; /** <The global configuration */

static struct node *node;

/* Test options */
static int running = 1; 	/**< Initiate shutdown if zero */
static int count =  -1;		/**< Amount of messages which should be sent (default: -1 for unlimited) */

/** File descriptor for Matlab results.
 * This allows you to write Matlab results in a seperate log file:
 *
 *    ./test etc/example.conf rtt -f 3 3>> measurement_results.m
 */
static int fd = STDOUT_FILENO;

/* Histogram */
static double low = 0;		/**< Lowest value in histogram. */
static double high = 2e-4;	/**< Highest value in histogram. */
static double res = 1e-5;	/**< Histogram resolution. */

#define CLOCK_ID	CLOCK_MONOTONIC

/* Prototypes */
void test_rtt();

void quit()
{
	running = 0;
}

void usage()
{
	printf("Usage: villas-test CONFIG TEST NODE [ARGS]\n");
	printf("  CONFIG  path to a configuration file\n");
	printf("  TEST    the name of the test to execute: 'rtt'\n");
	printf("  NODE    name of the node which shoud be used\n\n");

	print_copyright();
}

int main(int argc, char *argv[])
{
	if (argc < 4) {
		usage();
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

	log_init(&cfg.log, V, LOG_ALL);
	
	info("Parsing configuration");
	cfg_parse(&cfg, argv[1]);

	info("Initialize real-time system");
	rt_init(cfg.priority, cfg.affinity);
	
	info("Initialize memory system");
	memory_init();

	node = list_lookup(&cfg.nodes, argv[3]);
	if (!node)
		error("There's no node with the name '%s'", argv[3]);

	node_type_init(node->_vt, argc-3, argv+3, config_root_setting(&cfg.cfg));
	node_start(node);

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
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;

check:		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);
	}

	if (!strcmp(argv[2], "rtt"))
		test_rtt();
	else
		error("Unknown test: '%s'", argv[2]);

	node_stop(node);
	node_type_deinit(node->_vt);
	
	cfg_destroy(&cfg);

	return 0;
}

void test_rtt() {
	struct hist hist;
	
	struct timespec send, recv;

	struct sample *smp_send = alloc(SAMPLE_LEN(2));
	struct sample *smp_recv = alloc(SAMPLE_LEN(2));
	
	hist_create(&hist, low, high, res);

	/* Print header */
	fprintf(stdout, "%17s%5s%10s%10s%10s%10s%10s\n", "timestamp", "seq", "rtt", "min", "max", "mean", "stddev");

	while (running && (count < 0 || count--)) {		
		clock_gettime(CLOCK_ID, &send);

		node_write(node, &smp_send, 1); /* Ping */
		node_read(node,  &smp_recv, 1); /* Pong */
		
		clock_gettime(CLOCK_ID, &recv);

		double rtt = time_delta(&recv, &send);

		if (rtt < 0)
			warn("Negative RTT: %f", rtt);

		hist_put(&hist, rtt);

		smp_send->sequence++;

		fprintf(stdout, "%10lu.%06lu%5u%10.3f%10.3f%10.3f%10.3f%10.3f\n", 
			recv.tv_sec, recv.tv_nsec / 1000, smp_send->sequence,
			1e3 * rtt, 1e3 * hist.lowest, 1e3 * hist.highest,
			1e3 * hist_mean(&hist), 1e3 * hist_stddev(&hist));
	}

	struct stat st;
	if (!fstat(fd, &st)) {
		FILE *f = fdopen(fd, "w");
		hist_dump_matlab(&hist, f);
	}
	else
		error("Invalid file descriptor: %u", fd);

	hist_print(&hist, 1);

	hist_destroy(&hist);
}
