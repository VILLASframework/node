/** Receive messages from server snd print them on stdout.
 *
 * @file
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
 *
 * @addtogroup tools Test and debug tools
 * @{
 *********************************************************************************/

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <villas/super_node.h>
#include <villas/utils.h>
#include <villas/node.h>
#include <villas/msg.h>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/sample_io.h>
#include <villas/kernel/rt.h>

#include <villas/nodes/websocket.h>

#include "config.h"

static struct super_node sn = { .state = STATE_DESTROYED }; /**< The global configuration */

struct dir {
	struct pool pool;
	pthread_t thread;
	bool enabled;
	int limit;
} sendd, recvv;

struct node *node;

pthread_t ptid; /**< Parent thread id */

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	if (recvv.enabled) {
		pthread_cancel(recvv.thread);
		pthread_join(recvv.thread, NULL);
		pool_destroy(&recvv.pool);
	}

	if (sendd.enabled) {
		pthread_cancel(sendd.thread);
		pthread_join(sendd.thread, NULL);
		pool_destroy(&sendd.pool);
	}

	node_stop(node);
	node_destroy(node);
	
	if (node->_vt->start == websocket_start) {
		web_stop(&sn.web);
		api_stop(&sn.api);
	}

	info(GRN("Goodbye!"));
	exit(EXIT_SUCCESS);
}

static void usage()
{
	printf("Usage: villas-pipe [OPTIONS] CONFIG NODE\n");
	printf("  CONFIG  path to a configuration file\n");
	printf("  NODE    the name of the node to which samples are sent and received from\n");
	printf("  OPTIONS are:\n");
	printf("    -d LVL  set debug log level to LVL\n");
	printf("    -x      swap read / write endpoints\n");
	printf("    -s      only read data from stdin and send it to node\n");
	printf("    -r      only read data from node and write it to stdout\n");
	printf("    -L NUM  terminate after NUM samples sent\n");
	printf("    -l NUM  terminate after NUM samples received\n\n");

	print_copyright();
}

static void * send_loop(void *ctx)
{
	int ret, cnt = 0;
	struct sample *smps[node->vectorize];

	/* Initialize memory */
	ret = pool_init(&sendd.pool, LOG2_CEIL(node->vectorize), SAMPLE_LEN(DEFAULT_SAMPLELEN), &memtype_hugepage);
	if (ret < 0)
		error("Failed to allocate memory for receive pool.");

	ret = sample_alloc(&sendd.pool, smps, node->vectorize);
	if (ret < 0)
		error("Failed to get %u samples out of send pool (%d).", node->vectorize, ret);

	while (!feof(stdin)) {
		int len;
		for (len = 0; len < node->vectorize; len++) {
			struct sample *s = smps[len];
			int reason;
			
			if (sendd.limit > 0 && cnt >= sendd.limit)
				break;

retry:			reason = sample_io_villas_fscan(stdin, s, NULL);
			if (reason < 0) {
				if (feof(stdin))
					goto leave;
				else {
					warn("Skipped invalid message message: reason=%d", reason);
					goto retry;
				}
			}
		}

		cnt += node_write(node, smps, len);
		
		if (sendd.limit > 0 && cnt >= sendd.limit)
			goto leave2;

		pthread_testcancel();
	}

leave2:	info("Reached send limit. Terminating...");
	pthread_kill(ptid, SIGINT);

	return NULL;

	/* We reached EOF on stdin here. Lets kill the process */
leave:	if (recvv.limit < 0) {
		info("Reached end-of-file. Terminating...");
		pthread_kill(ptid, SIGINT);
	}

	return NULL;
}

static void * recv_loop(void *ctx)
{
	int ret, cnt = 0;
	struct sample *smps[node->vectorize];

	/* Initialize memory */
	ret = pool_init(&recvv.pool, LOG2_CEIL(node->vectorize), SAMPLE_LEN(DEFAULT_SAMPLELEN), &memtype_hugepage);
	if (ret < 0)
		error("Failed to allocate memory for receive pool.");

	ret = sample_alloc(&recvv.pool, smps, node->vectorize);
	if (ret  < 0)
		error("Failed to get %u samples out of receive pool (%d).", node->vectorize, ret);

	/* Print header */
	fprintf(stdout, "# %-20s\t\t%s\n", "sec.nsec+offset", "data[]");
	fflush(stdout);

	for (;;) {
		int recv = node_read(node, smps, node->vectorize);
		struct timespec now = time_now();

		for (int i = 0; i < recv; i++) {
			struct sample *s = smps[i];

			if (s->ts.received.tv_sec == -1 || s->ts.received.tv_sec == 0)
				s->ts.received = now;

			sample_io_villas_fprint(stdout, s, SAMPLE_IO_ALL);
			fflush(stdout);
		}
		
		cnt += recv;
		if (recvv.limit > 0 && cnt >= recvv.limit)
			goto leave;

		pthread_testcancel();
	}

leave:	info("Reached receive limit. Terminating...");
	pthread_kill(ptid, SIGINT);
	return NULL;

	return NULL;
}

int main(int argc, char *argv[])
{
	int ret, level = V;

	bool reverse = false;

	sendd = recvv = (struct dir) {
		.enabled = true,
		.limit = -1
	};

	ptid = pthread_self();
	
	char c, *endptr;
	while ((c = getopt(argc, argv, "hxrsd:l:L:")) != -1) {
		switch (c) {
			case 'x':
				reverse = true;
				break;
			case 's':
				recvv.enabled = false; // send only
				break;
			case 'r':
				sendd.enabled = false; // receive only
				break;
			case 'd':
				level = strtoul(optarg, &endptr, 10);
				goto check;
			case 'l':
				recvv.limit = strtoul(optarg, &endptr, 10);
				goto check;
			case 'L':
				sendd.limit = strtoul(optarg, &endptr, 10);
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

	if (argc != optind + 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	char *configfile = argv[optind];
	char *nodestr    = argv[optind+1];

	log_init(&sn.log, level, LOG_ALL);
	log_start(&sn.log);

	super_node_init(&sn);
	super_node_parse_uri(&sn, configfile);

	memory_init(sn.hugepages);
	signals_init(quit);
	rt_init(sn.priority, sn.affinity);

	/* Initialize node */
	node = list_lookup(&sn.nodes, nodestr);
	if (!node)
		error("Node '%s' does not exist!", nodestr);

	if (node->_vt->start == websocket_start) {
		web_start(&sn.web);
		api_start(&sn.api);
	}

	if (reverse)
		node_reverse(node);

	ret = node_type_start(node->_vt, &sn);
	if (ret)
		error("Failed to intialize node type: %s", node_type_name(node->_vt));

	ret = node_check(node);
	if (ret)
		error("Invalid node configuration");

	ret = node_start(node);
	if (ret)
		error("Failed to start node: %s", node_name(node));

	/* Start threads */
	if (recvv.enabled)
		pthread_create(&recvv.thread, NULL, recv_loop, NULL);
	
	if (sendd.enabled)
		pthread_create(&sendd.thread, NULL, send_loop, NULL);

	for (;;)
		sleep(1);

	return 0;
}

/** @} */
