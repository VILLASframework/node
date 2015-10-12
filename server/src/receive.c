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

#include "config.h"
#include "cfg.h"
#include "utils.h"
#include "node.h"
#include "msg.h"
#include "socket.h"
#include "timing.h"

/** Linked list of nodes */
struct list nodes;
/** The global configuration */
struct settings settings;

static struct settings set;
static struct msg *pool;
static struct node *node;

static void quit()
{
	node_stop(node);
	node_deinit();

	list_destroy(&nodes);
	free(pool);

	exit(EXIT_SUCCESS);
}

static void usage(char *name)
{
	printf("Usage: %s [-r] CONFIG NODE\n", name);
	printf("  -r      swap local / remote address of socket based nodes)\n\n");
	printf("  CONFIG  path to a configuration file\n");
	printf("  NODE    name of the node which shoud be used\n\n");

	printf("Simulator2Simulator Server %s (built on %s %s)\n",
		BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
	printf(" copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC\n");
	printf(" Steffen Vogel <StVogel@eonerc.rwth-aachen.de>\n");

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int reverse = 0;

	config_t config;

	_mtid = pthread_self();

	char c;
	while ((c = getopt(argc, argv, "hr")) != -1) {
		switch (c) {
			case 'r': reverse = 1; break;
			case 'h':
			case '?': usage(argv[0]);
		}
	}

	if (argc - optind != 2)
		usage(argv[0]);

	/* Setup signals */
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT, &sa_quit, NULL);

	list_init(&nodes, (dtor_cb_t) node_destroy);
	
	log_init();
	config_init(&config);
	config_parse(argv[optind], &config, &set, &nodes, NULL);

	node = list_lookup(&nodes, argv[optind+1]);
	if (!node)
		error("There's no node with the name '%s'", argv[optind+1]);

	if (reverse)
		node_reverse(node);

	node->refcnt++;
	node->_vt->refcnt++;

	node_init(argc-optind, argv+optind, &set);
	node_start(node);

	pool = alloc(sizeof(struct msg) * node->combine);

	/* Print header */
	fprintf(stderr, "# %-20s\t\t%s\n", "sec.nsec+offset(seq)", "data[]");

	for (;;) {
		struct timespec ts = time_now();
		
		int recv = node_read(node, pool, node->combine, 0, node->combine);
		for (int i = 0; i < recv; i++) {
			struct msg *m = &pool[i];
			
			int ret = msg_verify(m);
			if (ret)
				warn("Failed to verify message: %d", ret);
			
			/** @todo should we drop reordered / delayed packets here? */

			msg_fprint(stdout, &pool[i], MSG_PRINT_ALL, time_delta(&MSG_TS(m), &ts));
		}
	}

	return 0;
}

/** @} */
