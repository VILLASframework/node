/** Receive messages from server snd print them on stdout.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *
 * @addtogroup tools Test and debug tools
 * @{
 *********************************************************************************/

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include "config.h"
#include "cfg.h"
#include "utils.h"
#include "node.h"
#include "msg.h"
#include "timing.h"
#include "pool.h"


struct list nodes;		/**< List of all nodes */
struct settings settings;	/**< The global configuration */

struct pool recv_pool,   send_pool;
pthread_t   recv_thread, send_thread;

struct node *node;

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	pthread_cancel(recv_thread);
	pthread_cancel(send_thread);

	pthread_join(recv_thread, NULL);
	pthread_join(send_thread, NULL);

	node_stop(node);
	node_deinit(node->_vt);

	pool_destroy(&recv_pool);
	pool_destroy(&send_pool);

	list_destroy(&nodes, (dtor_cb_t) node_destroy, false);

	info(GRN("Goodbye!"));
	exit(EXIT_SUCCESS);
}

static void usage(char *name)
{
	printf("Usage: %s CONFIG [-r] NODE\n", name);
	printf("  CONFIG  path to a configuration file\n");
	printf("  NODE    the name of the node to which samples are sent and received from\n");
	printf("  -d LVL  set debug log level to LVL\n");
	printf("  -x      swap read / write endpoints\n");
	printf("  -s      only read data from stdin and send it to node\n");
	printf("  -r      only read data from node and write it to stdout\n\n");

	print_copyright();

	exit(EXIT_FAILURE);
}

void * send_loop(void *ctx)
{
	int ret;
	struct sample *smps[node->vectorize];

	/* Initialize memory */
	ret = pool_init_mmap(&send_pool, SAMPLE_LEN(DEFAULT_VALUES), node->vectorize);
	if (ret < 0)
		error("Failed to allocate memory for receive pool.");

	ret = sample_get_many(&send_pool, smps, node->vectorize);
	if (ret < 0)
		error("Failed to get %u samples out of send pool (%d).", node->vectorize, ret);

	for (;;) {
		for (int i = 0; i < node->vectorize; i++) {
			struct sample *s = smps[i];
			int reason, retry;

			do {
				retry = 0;
				reason = sample_fscan(stdin, s, NULL);
				if (reason < 0) {
					if (feof(stdin))
						return NULL;
					else {
						warn("Skipped invalid message message from stdin: reason=%d", reason);
						retry = 1;
					}
				}
			} while (retry);
		}

		node_write(node, smps, node->vectorize);
	}

	return NULL;
}

void * recv_loop(void *ctx)
{
	int ret;
	struct sample *smps[node->vectorize];

	/* Initialize memory */
	ret = pool_init_mmap(&recv_pool, SAMPLE_LEN(DEFAULT_VALUES), node->vectorize);
	if (ret < 0)
		error("Failed to allocate memory for receive pool.");

	ret = sample_get_many(&recv_pool, smps, node->vectorize);
	if (ret  < 0)
		error("Failed to get %u samples out of receive pool (%d).", node->vectorize, ret);

	/* Print header */
	fprintf(stdout, "# %-20s\t\t%s\n", "sec.nsec+offset", "data[]");

	for (;;) {
		int recv = node_read(node, smps, node->vectorize);
		for (int i = 0; i < recv; i++) {
			struct sample *s = smps[i];

			sample_fprint(stdout, s, SAMPLE_ALL);
			fflush(stdout);
		}
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	bool send = true, recv = true, reverse = false;

	/* Parse command line arguments */
	if (argc < 3)
		usage(argv[0]);

	log_init();

	char c;
	while ((c = getopt(argc-2, argv+2, "hxrsd:")) != -1) {
		switch (c) {
			case 'x':
				reverse = true;
				break;
			case 's':
				recv = false;
				break;
			case 'r':
				send = false;
				break;
			case 'd':
				log_setlevel(atoi(optarg), -1);
				break;
			case 'h':
			case '?':
				usage(argv[0]);
		}
	}

	/* Setup signals */
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT,  &sa_quit, NULL);

	/* Initialize log, configuration.. */
	config_t config;

	/* Create lists */
	list_init(&nodes);

	config_init(&config);
	config_parse(argv[1], &config, &settings, &nodes, NULL);

	/* Initialize node */
	node = list_lookup(&nodes, argv[2]);
	if (!node)
		error("Node '%s' does not exist!", argv[2]);

	if (reverse)
		node_reverse(node);

	node_init(node->_vt, argc-optind, argv+optind, config_root_setting(&config));

	node_start(node);

	/* Start threads */
	if (recv)
		pthread_create(&recv_thread, NULL, recv_loop, NULL);
	if (send)
		pthread_create(&send_thread, NULL, send_loop, NULL);

	for (;;)
		pause();

	return 0;
}

/** @} */
