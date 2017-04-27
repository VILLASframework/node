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
	bool started;
} sendd, recvv;

bool reverse = false;

struct node *node;

pthread_t ptid; /**< Parent thread id */

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	int ret;

	if (recvv.started) {
		pthread_cancel(recvv.thread);
		pthread_join(recvv.thread, NULL);
		pool_destroy(&recvv.pool);
	}
	
	if (sendd.started) {
		pthread_cancel(sendd.thread);
		pthread_join(sendd.thread, NULL);
		pool_destroy(&sendd.pool);
	}
	
	ret = super_node_stop(&sn);
	if (ret)
		error("Failed to stop super-node");
	
	super_node_destroy(&sn);

	info(GRN("Goodbye!"));
	exit(EXIT_SUCCESS);
}

static void usage()
{
	printf("Usage: villas-pipe CONFIG NODE [OPTIONS]\n");
	printf("  CONFIG  path to a configuration file\n");
	printf("  NODE    the name of the node to which samples are sent and received from\n");
	printf("  OPTIONS are:\n");
	printf("    -d LVL  set debug log level to LVL\n");
	printf("    -x      swap read / write endpoints\n");
	printf("    -s      only read data from stdin and send it to node\n");
	printf("    -r      only read data from node and write it to stdout\n\n");

	print_copyright();

	exit(EXIT_FAILURE);
}

static void * send_loop(void *ctx)
{
	int ret;
	struct sample *smps[node->vectorize];

	if (!sendd.enabled)
		return NULL;
	
	sendd.started = true;
	
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

retry:			reason = sample_io_villas_fscan(stdin, s, NULL);
			if (reason < 0) {
				if (feof(stdin))
					break;
				else {
					warn("Skipped invalid message message: reason=%d", reason);
					goto retry;
				}
			}
		}

		node_write(node, smps, len);
		pthread_testcancel();
	}

	/* We reached EOF on stdin here. Lets kill the process */
	info("Reached end-of-file. Terminating...");
	pthread_kill(ptid, SIGINT);

	return NULL;
}

static void * recv_loop(void *ctx)
{
	int ret;
	struct sample *smps[node->vectorize];
	
	if (!recvv.enabled)
		return NULL;

	recvv.started = true;
	
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
		pthread_testcancel();
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	int ret, level = V;
	char c;

	ptid = pthread_self();

	/* Default values */
	sendd.enabled = true;
	recvv.enabled = true;

	while ((c = getopt(argc, argv, "hxrsd:")) != -1) {
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
				level = atoi(optarg);
				break;
			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}
	}
	
	if (argc < optind + 2) {
		usage();
		exit(EXIT_FAILURE);
	}
	
	log_init(&sn.log, level, LOG_ALL);
	log_start(&sn.log);
	
	super_node_init(&sn);
	super_node_parse_uri(&sn, argv[optind]);
	
	memory_init(sn.hugepages);
	signals_init(quit);
	rt_init(sn.priority, sn.affinity);

	/* Initialize node */
	node = list_lookup(&sn.nodes, argv[optind+1]);
	if (!node)
		error("Node '%s' does not exist!", argv[optind+1]);

	if (node->_vt->start == websocket_start) {
		web_start(&sn.web);
		api_start(&sn.api);
	}

	if (reverse)
		node_reverse(node);

	ret = node_type_start(node->_vt, &sn);
	if (ret)
		error("Failed to intialize node type: %s", node_name(node));
	
	ret = node_check(node);
	if (ret)
		error("Invalid node configuration");

	ret = node_start(node);
	if (ret)
		error("Failed to start node: %s", node_name(node));

	/* Start threads */
	pthread_create(&recvv.thread, NULL, recv_loop, NULL);
	pthread_create(&sendd.thread, NULL, send_loop, NULL);

	for (;;)
		sleep(1);

	return 0;
}

/** @} */
