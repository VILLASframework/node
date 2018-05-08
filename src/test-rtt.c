/** Measure round-trip time.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#include <villas/config.h>
#include <villas/super_node.h>
#include <villas/node.h>
#include <villas/utils.h>
#include <villas/hist.h>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/kernel/rt.h>

struct super_node sn; /** <The global configuration */

static struct node *node;

/* Test options */
static int running = 1; 	/**< Initiate shutdown if zero */
static int count =  -1;		/**< Amount of messages which should be sent (default: -1 for unlimited) */

static hist_cnt_t hist_warmup;
static int hist_buckets;

/** File descriptor for Matlab results.
 * This allows you to write Matlab results in a seperate log file:
 *
 *    ./test etc/example.conf rtt -f 3 3>> measurement_results.m
 */
static int fd = STDOUT_FILENO;

#define CLOCK_ID	CLOCK_MONOTONIC

/* Prototypes */
void test_rtt();

void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	running = 0;
}

void usage()
{
	printf("Usage: villas-test-rtt [OPTIONS] CONFIG NODE\n");
	printf("  CONFIG  path to a configuration file\n");
	printf("  NODE    name of the node which shoud be used\n");
	printf("  OPTIONS is one or more of the following options:\n");
	printf("    -c CNT  send CNT messages\n");
	printf("    -f FD   use file descriptor FD for result output instead of stdout\n");
	printf("    -b BKTS number of buckets for histogram\n");
	printf("    -w WMUP duration of histogram warmup phase\n");
	printf("    -h      show this usage information\n");
	printf("    -V      show the version of the tool\n\n");
	printf("\n");

	print_copyright();
}

int main(int argc, char *argv[])
{
	/* Parse Arguments */
	char c, *endptr;
	while ((c = getopt (argc, argv, "w:h:r:f:c:b:V")) != -1) {
		switch (c) {
			case 'c':
				count = strtoul(optarg, &endptr, 10);
				goto check;
			case 'f':
				fd = strtoul(optarg, &endptr, 10);
				goto check;
			case 'w':
				hist_warmup = strtoul(optarg, &endptr, 10);
				goto check;
			case 'b':
				hist_buckets = strtoul(optarg, &endptr, 10);
				goto check;
			case 'V':
				print_version();
				exit(EXIT_SUCCESS);
			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;

check:		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);
	}

	if (argc != optind + 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	char *configfile = argv[optind];
	char *nodestr    = argv[optind + 1];

	log_init(&sn.log, V, LOG_ALL);

	super_node_init(&sn);
	super_node_parse_uri(&sn, configfile);

	signals_init(quit);
	rt_init(sn.priority, sn.affinity);
	memory_init(sn.hugepages);

	node = list_lookup(&sn.nodes, nodestr);
	if (!node)
		error("There's no node with the name '%s'", nodestr);

	node_type_start(node->_vt, &sn);
	node_start(node);

	test_rtt();

	node_stop(node);
	node_type_stop(node->_vt);

	super_node_destroy(&sn);

	return 0;
}

void test_rtt() {
	struct hist hist;

	struct timespec send, recv;

	struct sample *smp_send = (struct sample *) alloc(SAMPLE_LEN(2));
	struct sample *smp_recv = (struct sample *) alloc(SAMPLE_LEN(2));

	hist_init(&hist, 20, 100);

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
