/** Receive messages from server snd print them on stdout.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "config.h"
#include "cfg.h"
#include "utils.h"
#include "node.h"
#include "msg.h"

static struct settings set;
static struct msg msg = MSG_INIT(0);
extern struct node *nodes;
static struct node *node;

void quit(int sig, siginfo_t *si, void *ptr)
{
	node_stop(node);
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	struct config_t config;

	if (argc != 2) {
		printf("Usage: %s CONFIG NODE\n", argv[0]);
		printf("  CONFIG  path to a configuration file\n");
		printf("  NODE    name of the node which shoud be used\n\n");
		printf("Simulator2Simulator Server %s (built on %s %s)\n",
			BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
		printf(" Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		printf("   Steffen Vogel <stvogel@eonerc.rwth-aachen.de>\n");
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

	config_init(&config);
	config_parse(argv[1], &config, &set, &nodes, NULL);
	
	node = node_lookup_name(argv[2], nodes);
	if (!node)
		error("There's no node with the name '%s'", argv[2]);

	node_start(node);
	node_start_defer(node);

	/* Print header */
	fprintf(stderr, "# %-6s %-8s %-12s\n", "dev_id", "seq_no", "data");

	while (1) {
		node_read(node, &msg);

		if (msg.version != MSG_VERSION)
			continue;
		if (msg.type != MSG_TYPE_DATA)
			continue;

#if 1
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		fprintf(stdout, "%17.6f", ts.tv_sec + ts.tv_nsec / 1e9);
#endif

		msg_fprint(stdout, &msg);
	}

	return 0;
}
