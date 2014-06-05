/**
 * Main routine
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <stdio.h>
#include <error.h>

#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "config.h"
#include "cfg.h"
#include "msg.h"
#include "utils.h"
#include "path.h"
#include "node.h"

/** Default settings */
static struct config config = {
	.priority = 0,
	.affinity = 0xC0,
	.protocol = 0
};

static void start()
{
	/* Connect and bind nodes to their sockets, set socket options */
	for (int i = 0; i < config.node_count; i++) {
		struct node *n = &config.nodes[i];

		node_connect(n);

		debug(1, "  We listen for node '%s' at %s:%u", n->name, inet_ntoa(n->local.sin_addr), ntohs(n->local.sin_port));
		debug(1, "  We sent to node '%s' at %s:%u", n->name, inet_ntoa(n->remote.sin_addr), ntohs(n->remote.sin_port));
	}

	/* Start on thread per path for asynchronous processing */
	for (int i = 0; i < config.path_count; i++) {
		struct path *p = &config.paths[i];

		path_start(p);

		info("Starting path: %12s => %s => %-12s", p->in->name, config.name, p->out->name);
	}
}

static void stop()
{
	/* Join all threads and print statistics */
	for (int i = 0; i < config.path_count; i++) {
		struct path *p = &config.paths[i];

		path_stop(p);

		info("Stopping path: %12s => %s => %-12s", p->in->name, config.name, p->out->name);
		info("  %u messages received", p->received);
		info("  %u messages duplicated", p->duplicated);
		info("  %u messages delayed", p->delayed);
	}

	/* Close all sockets we listing on */
	for (int i = 0; i < config.node_count; i++) {
		struct node *n = &config.nodes[i];

		node_disconnect(n);
	}
}

static void quit()
{
	stop();

	free(config.paths);
	free(config.nodes);
	config_destroy(&config.obj);

	_exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	/* Setup signals */
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT, &sa_quit, NULL);
	atexit(&quit);

	/* Check arguments */
	if (argc != 2) {
		printf("Usage: %s CONFIG\n", argv[0]);
		printf("  CONFIG is a required path to a configuration file\n\n");
		printf("Simulator2Simulator Server %s (%s %s)\n", VERSION, __DATE__, __TIME__);
		printf(" Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		printf("   Steffen Vogel <stvogel@eonerc.rwth-aachen.de>\n\n");
		exit(EXIT_FAILURE);
	}

	info("This is s2ss %s", VERSION);

	/* Parse configuration file */
	config.filename = argv[1];
	config_init(&config.obj);
	config_parse(&config.obj, &config);

	if (!config.path_count)
		error("No paths found. Terminating...");
	else
		info("Parsed %u nodes and %u paths", config.node_count, config.path_count);

	/* Setup various realtime related things */
	init_realtime(&config);

	/* Connect all nodes to their sockets and start one thread per path */
	start();

	/* Main thread is sleeping */
	while (1) pause();

	/* Stop and free ressources */
	quit();

	return 0;
}
