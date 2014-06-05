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

#include "msg.h"
#include "utils.h"
#include "config.h"
#include "path.h"
#include "node.h"

static struct node *nodes[MAX_NODES] = { NULL };
static struct path *paths[MAX_PATHS] = { NULL };

int dumper(struct msg *m)
{
	msg_fprint(stdout, m);
}

/**
 * Do your configuration here
 */
void init()
{
	nodes[0] = node_create("test", SERVER, "*:10201", "localhost:10200");
	//nodes[1] = node_create("sintef", SERVER, "localhost", 10201);

	paths[0] = path_create(nodes[0], nodes[0]);

	path_start(paths[0]);
	paths[0]->hooks[0] = dumper;

	//paths[1] = path_create(nodes[1], &nodes[0], 1);

	//for (int i = 0; i < MAX_PATHS && paths[i]; i++) {
	//	path_start(paths[i]);
	//}
}

void quit()
{
	for (int i = 0; i < MAX_PATHS && paths[i]; i++) {
		path_stop(paths[i]);
		path_destroy(paths[i]);
	}

	for (int i = 0; i < MAX_NODES && nodes[i]; i++)
		node_destroy(nodes[i]);

	debug(1, "Goodbye!");
	_exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	atexit(&quit);

	if (argc != 1) {
		printf("Usage: %s [config]\n", argv[0]);
		printf("  config is an optional path to a configuration file\n\n");
		printf("Simulator2Simulator Server %s (%s %s)\n", VERSION, __DATE__, __TIME__);
		printf(" Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		printf("   Steffen Vogel <stvogel@eonerc.rwth-aachen.de>\n\n");
		exit(EXIT_FAILURE);
	}

	debug(1, "Good morning! This is s2ss %s", VERSION);

	init(); /* Setup paths and nodes manually */

	signal(SIGINT, quit);
	pause();

	print(INFO, "Good night!");

	return 0;
}
