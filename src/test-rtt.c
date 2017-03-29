/** Measure round-trip time.
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

#include <villas/super_node.h>
#include <villas/node.h>
#include <villas/utils.h>
#include <villas/hist.h>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/kernel/rt.h>

#include "config.h"

struct super_node sn; /** <The global configuration */

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

void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	running = 0;
}

void usage()
{
	printf("Usage: villas-test-rtt CONFIG NODE [ARGS]\n");
	printf("  CONFIG  path to a configuration file\n");
	printf("  NODE    name of the node which shoud be used\n");
	printf("  ARGS    the following optional options:\n");
	printf("   -c CNT  send CNT messages\n");
	printf("   -f FD   use file descriptor FD for result output instead of stdout\n");
	printf("   -l LOW  smallest value for histogram\n");
	printf("   -H HIGH largest value for histogram\n");
	printf("   -r RES  bucket resolution for histogram\n");
	printf("   -h      show this usage information\n");
	printf("\n");

	print_copyright();
}

int main(int argc, char *argv[])
{
	if (argc < 4) {
		usage();
		exit(EXIT_FAILURE);
	}

	log_init(&sn.log, V, LOG_ALL);
	
	super_node_init(&sn);
	super_node_parse_uri(&sn, argv[1]);
	
	signals_init(quit);
	rt_init(sn.priority, sn.affinity);
	memory_init(sn.hugepages);

	node = list_lookup(&sn.nodes, argv[3]);
	if (!node)
		error("There's no node with the name '%s'", argv[3]);

	node_type_start(node->_vt, argc-3, argv+3, config_root_setting(&sn.cfg));
	node_start(node);

	/* Parse Arguments */
	char c, *endptr;
	while ((c = getopt (argc-3, argv+3, "l:hH:r:f:c:")) != -1) {
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
			case 'H':
				high = strtod(optarg, &endptr);
				goto check;
			case 'r':
				res = strtod(optarg, &endptr);
				goto check;
			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;

check:		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);
	}

	test_rtt();

	node_stop(node);
	node_type_stop(node->_vt);
	
	super_node_destroy(&sn);

	return 0;
}

void test_rtt() {
	struct hist hist;
	
	struct timespec send, recv;

	struct sample *smp_send = alloc(SAMPLE_LEN(2));
	struct sample *smp_recv = alloc(SAMPLE_LEN(2));
	
	hist_init(&hist, low, high, res);

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
