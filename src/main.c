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

/**
 * Do your configuration here
 */
void init()
{
	nodes[0] = node_create("opal", SERVER, "localhost", 10200);
	nodes[1] = node_create("sintef", SERVER, "localhost", 10201);

	paths[0] = path_create(nodes[0], &nodes[1], 1);
	paths[1] = path_create(nodes[1], &nodes[0], 1);

	for (int i = 0; i < MAX_PATHS && paths[i]; i++) {
		path_start(paths[i]);
	}
}

void quit()
{
	for (int i = 0; i < MAX_PATHS && paths[i]; i++) {
		path_stop(paths[i]);
		path_destroy(paths[i]);
	}

	for (int i = 0; i < MAX_NODES && nodes[i]; i++) {
		node_destroy(nodes[i]);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 1) {
		printf("Usage: s2ss [config]\n");
		printf("  config is an optional path to a configuration file\n\n");
		printf("s2ss Simulator2Simulator Server v%s\n", VERSION);
		printf("Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		exit(EXIT_FAILURE);
	}

	print(INFO, "Good morning! This is s2ss v%s", VERSION);

	init();
	signal(SIGINT, quit);
	pause();

	print(INFO, "Good night!");

	return 0;
}
