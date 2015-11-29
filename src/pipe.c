/** Receive messages from server snd print them on stdout.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *
 * @addtogroup tools Test and debug tools
 * @{
 *********************************************************************************/

#include <stdlib.h>
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

/** Linked list of nodes */
struct list nodes = LIST_INIT((dtor_cb_t) node_destroy);

/** The global configuration */
struct settings settings;

struct msg *recv_pool,  *send_pool;
pthread_t   recv_thread, send_thread;

struct node *node;
int reverse;

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	pthread_join(recv_thread, NULL);
	pthread_join(send_thread, NULL);
		
	node_stop(node);
	node_deinit();
		
	free(recv_pool);
	free(send_pool);
	
	list_destroy(&nodes);
	
	info(GRN("Goodbye!"));
	exit(EXIT_SUCCESS);
}

static void usage(char *name)
{
	printf("Usage: %s CONFIG [-r] NODE\n", name);
	printf("  CONFIG  path to a configuration file\n");
	printf("  NODE    the name of the node to which samples are sent and received from\n");
	printf("  -r      swap read / write endpoints)\n\n");

	printf("Simulator2Simulator Server %s (built on %s %s)\n",
		BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
	printf(" copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC\n");
	printf(" Steffen Vogel <StVogel@eonerc.rwth-aachen.de>\n");

	exit(EXIT_FAILURE);
}

void * send_loop(void *ctx)
{	
	for (;;) {
		for (int i = 0; i < node->combine; i++) {
			struct msg *m = &send_pool[i];
			int reason;

retry:			reason = msg_fscan(stdin, m, NULL, NULL);
			if (reason < 0) {
				if (feof(stdin))
					return NULL;
				else {
					warn("Skipped invalid message message: reason=%d", reason);
					goto retry;
				}
			}
		}

		node_write(node, send_pool, node->combine, 0, node->combine);
	}

	return NULL;
}

void * recv_loop(void *ctx)
{
	/* Print header */
	fprintf(stdout, "# %-20s\t\t%s\n", "sec.nsec+offset(seq)", "data[]");

	for (;;) {
		struct timespec ts = time_now();
		
		int recv = node_read(node, recv_pool, node->combine, 0, node->combine);
		for (int i = 0; i < recv; i++) {
			struct msg *m = &recv_pool[i];
			
			int ret = msg_verify(m);
			if (ret)
				warn("Failed to verify message: %d", ret);
			
			/** @todo should we drop reordered / delayed packets here? */

			msg_fprint(stdout, m, MSG_PRINT_ALL, time_delta(&MSG_TS(m), &ts));
		}
	}
	
	return NULL;
}

int main(int argc, char *argv[])
{
	/* Parse command line arguments */
	if (argc < 2)
		usage(argv[0]);
	
	char c;
	while ((c = getopt(argc-2, argv+2, "hr")) != -1) {
		switch (c) {
			case 'r':
				reverse = 1;
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

	log_init();
	config_init(&config);
	config_parse(argv[1], &config, &settings, &nodes, NULL);
	
	/* Initialize node */
	node = list_lookup(&nodes, argv[2]);
	if (!node)
		error("Node '%s' does not exist!", argv[2]);

	node_init(argc-optind, argv+optind, &settings);
	
	recv_pool = alloc(sizeof(struct msg) * node->combine);
	send_pool = alloc(sizeof(struct msg) * node->combine);
	
	if (reverse)
		node_reverse(node);
	
	node_start(node);
	
	/* Start threads */
	pthread_create(&recv_thread, NULL, recv_loop, NULL);
	pthread_create(&send_thread, NULL, send_loop, NULL);
	
	for (;;) pause();

	return 0;
}

/** @} */
