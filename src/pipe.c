/** Receive messages from server snd print them on stdout.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *
 * @addtogroup tools Test and debug tools
 * @{
 *********************************************************************************/

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <villas/cfg.h>
#include <villas/utils.h>
#include <villas/node.h>
#include <villas/msg.h>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/kernel/rt.h>

#include "config.h"

static struct cfg cfg;		/**< The global configuration */

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

	node_stop(node);
	node_deinit(node->_vt);

	cfg_destroy(&cfg);

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
	ret = pool_init(&sendd.pool, LOG2_CEIL(node->vectorize), SAMPLE_LEN(DEFAULT_VALUES), &memtype_hugepage);
	if (ret < 0)
		error("Failed to allocate memory for receive pool.");
	
	ret = sample_alloc(&sendd.pool, smps, node->vectorize);
	if (ret < 0)
		error("Failed to get %u samples out of send pool (%d).", node->vectorize, ret);

	for (;;) {
		for (int i = 0; i < node->vectorize; i++) {
			struct sample *s = smps[i];
			int reason;

retry:			reason = sample_fscan(stdin, s, NULL);
			if (reason < 0) {
				if (feof(stdin))
					goto killme;
				else {
					warn("Skipped invalid message message: reason=%d", reason);
					goto retry;
				}
			}
		}

		node_write(node, smps, node->vectorize);
		pthread_testcancel();
	}

killme: pthread_kill(ptid, SIGINT);

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
	ret = pool_init(&recvv.pool, LOG2_CEIL(node->vectorize), SAMPLE_LEN(DEFAULT_VALUES), &memtype_hugepage);
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

			sample_fprint(stdout, s, SAMPLE_ALL);
			fflush(stdout);
		}
		pthread_testcancel();
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	int ret;
	char c;

	ptid = pthread_self();

	/* Parse command line arguments */
	if (argc < 3)
		usage();

	/* Default values */
	sendd.enabled = true;
	recvv.enabled = true;

	while ((c = getopt(argc-2, argv+2, "hxrsd:")) != -1) {
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
				cfg.log.level = atoi(optarg);
				break;
			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}
	}
	
	log_init(&cfg.log, cfg.log.level, LOG_ALL);

	/* Setup signals */
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT,  &sa_quit, NULL);

	/* Initialize log, configuration.. */
	info("Parsing configuration");
	cfg_parse(&cfg, argv[1]);

	info("Initialize real-time system");
	rt_init(cfg.priority, cfg.affinity);
	
	info("Initialize memory system");
	memory_init();
	
	/* Initialize node */
	node = list_lookup(&cfg.nodes, argv[2]);
	if (!node)
		error("Node '%s' does not exist!", argv[2]);

	if (reverse)
		node_reverse(node);

	ret = node_init(node->_vt, argc-optind, argv+optind, config_root_setting(&cfg.cfg));
	if (ret)
		error("Failed to intialize node: %s", node_name(node));

	ret = node_start(node);
	if (ret)
		error("Failed to start node: %s", node_name(node));

	/* Start threads */
	pthread_create(&recvv.thread, NULL, recv_loop, NULL);
	pthread_create(&sendd.thread, NULL, send_loop, NULL);

	for (;;)
		pause();

	return 0;
}

/** @} */
