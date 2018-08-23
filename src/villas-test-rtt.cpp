/** Measure round-trip time.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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
#include <inttypes.h>
#include <iostream>

#include <villas/super_node.h>
#include <villas/node/config.h>
#include <villas/node.h>
#include <villas/utils.h>
#include <villas/hist.h>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/kernel/rt.h>

using namespace villas::node;

SuperNode sn; /** <The global configuration */

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
	std::cout << "Usage: villas-test-rtt [OPTIONS] CONFIG NODE" << std::endl;
	std::cout << "  CONFIG  path to a configuration file" << std::endl;
	std::cout << "  NODE    name of the node which shoud be used" << std::endl;
	std::cout << "  OPTIONS is one or more of the following options:" << std::endl;
	std::cout << "    -c CNT  send CNT messages" << std::endl;
	std::cout << "    -f FD   use file descriptor FD for result output instead of stdout" << std::endl;
	std::cout << "    -b BKTS number of buckets for histogram" << std::endl;
	std::cout << "    -w WMUP duration of histogram warmup phase" << std::endl;
	std::cout << "    -h      show this usage information" << std::endl;
	std::cout << "    -V      show the version of the tool" << std::endl << std::endl;

	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret;

	/* Parse Arguments */
	int c;
	char *endptr;
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

	ret = signals_init(quit);
	if (ret)
		error("Failed to initialize signals subsystem");

	ret = sn.parseUri(configfile);
	if (ret)
		error("Failed to parse configuration");

	ret = sn.init();
	if (ret)
		error("Initialization failed!");

	ret = log_open(sn.getLog());
	if (ret)
		error("Failed to open log");

	node = sn.getNode(nodestr);
	if (!node)
		error("There's no node with the name '%s'", nodestr);

	ret = node_type_start(node->_vt);//, &sn); // @todo: port to C++
	if (ret)
		error("Failed to start node-type %s: reason=%d", node_type_name(node->_vt), ret);

	ret = node_init2(node);
	if (ret)
		error("Failed to start node %s: reason=%d", node_name(node), ret);

	ret = node_start(node);
	if (ret)
		error("Failed to start node %s: reason=%d", node_name(node), ret);

	test_rtt();

	ret = node_stop(node);
	if (ret)
		error("Failed to stop node %s: reason=%d", node_name(node), ret);

	ret = node_type_stop(node->_vt);
	if (ret)
		error("Failed to stop node-type %s: reason=%d", node_type_name(node->_vt), ret);

	return 0;
}

void test_rtt() {
	struct hist hist;

	struct timespec send, recv;

	struct sample *smp_send = (struct sample *) alloc(SAMPLE_LENGTH(2));
	struct sample *smp_recv = (struct sample *) alloc(SAMPLE_LENGTH(2));

	hist_init(&hist, 20, 100);

	/* Print header */
	fprintf(stdout, "%17s%5s%10s%10s%10s%10s%10s\n", "timestamp", "seq", "rtt", "min", "max", "mean", "stddev");

	while (running && (count < 0 || count--)) {
		clock_gettime(CLOCK_ID, &send);

		unsigned release;

		release = 1; // release = allocated
		node_write(node, &smp_send, 1, &release); /* Ping */

		release = 1; // release = allocated
		node_read(node,  &smp_recv, 1, &release); /* Pong */

		clock_gettime(CLOCK_ID, &recv);

		double rtt = time_delta(&recv, &send);

		if (rtt < 0)
			warn("Negative RTT: %f", rtt);

		hist_put(&hist, rtt);

		smp_send->sequence++;

		fprintf(stdout, "%10lu.%06lu%5" PRIu64 "%10.3f%10.3f%10.3f%10.3f%10.3f\n",
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
