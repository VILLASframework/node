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

#include "config.h"
#include "cfg.h"
#include "msg.h"
#include "utils.h"
#include "path.h"
#include "node.h"

/// Global settings
struct config config;

void start()
{
	for (int i = 0; i < config.node_count; i++) {
		struct node *n = &config.nodes[i];

		node_connect(n);
	}

	for (int i = 0; i < config.path_count; i++) {
		struct path *p = &config.paths[i];

		path_start(p);

		info("Starting path: %12s => %s => %-12s", p->in->name, config.name, p->out->name);
	}
}

void stop()
{
	for (int i = 0; i < config.path_count; i++) {
		struct path *p = &config.paths[i];

		path_stop(p);

		info("Stopping path: %12s => %s => %-12s", p->in->name, config.name, p->out->name);
		info("  %u messages received", p->received);
		info("  %u messages duplicated", p->duplicated);
		info("  %u messages delayed", p->delayed);
	}

	for (int i = 0; i < config.node_count; i++) {
		struct node *n = &config.nodes[i];

		node_disconnect(n);
	}
}

void quit()
{
	/* Stop and disconnect all paths/nodes */
	stop();

	free(config.paths);
	free(config.nodes);
	config_destroy(&config.obj);

	_exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	atexit(&quit);

	if (argc != 2) {
		printf("Usage: %s CONFIG\n", argv[0]);
		printf("  CONFIG is a required path to a configuration file\n\n");
		printf("Simulator2Simulator Server %s (%s %s)\n", VERSION, __DATE__, __TIME__);
		printf(" Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		printf("   Steffen Vogel <stvogel@eonerc.rwth-aachen.de>\n\n");
		exit(EXIT_FAILURE);
	}

	info("This is s2ss %s", VERSION);

	// Default settings
	config.filename = argv[1];
	config.debug = 0;
	config.nice = 0;
	config.affinity = 0xC0;
	config.protocol = 0;

	config_init(&config.obj);
	config_parse(&config.obj, &config);

	if (config.path_count)
		info("Parsed %u nodes and %u paths", config.node_count, config.path_count);
	else
		error("No paths found. Terminating...");

	/* Start and connect all paths/nodes */
	start();

	/* Setup signals */
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT, &sa_quit, NULL);

	/* Main thread is sleeping */
	while (1) pause();

	/* Stop and free ressources */
	quit();

	return 0;
}
