/** Main routine
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <stdio.h>
#include <error.h>

#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "cfg.h"
#include "if.h"
#include "msg.h"
#include "utils.h"
#include "path.h"
#include "node.h"

/** Linked list of nodes */
static struct node *nodes;
/** Linked list of paths */
static struct path *paths;
/** Linked list of interfaces */
static struct interface *interfaces;

/** The global configuration */
static struct settings settings;
static config_t config;

static void start()
{
	/* Configure interfaces */
	for (struct interface *i = interfaces; i; i = i->next) {
		if_indextoname(i->index, i->name);

		debug(3, "Setup interface '%s'",
			i->name, i->index, i->refcnt);

		if (settings.affinity) {
			if_getirqs(i);
			if_setaffinity(i, settings.affinity);
		}

		/* Create priority queuing discipline */
		tc_prio(i, TC_HDL(4000, 0), i->refcnt);
	}

	/* Connect and bind nodes to their sockets, set socket options */
	for (struct node *n = nodes; n; n = n->next) {
		node_connect(n);

		/* Create queueing discipline */
		if (n->netem) {
			tc_mark(n->interface, TC_HDL(4000, n->mark), n->mark);
			tc_netem(n->interface, TC_HDL(4000, n->mark), n->netem);
		}

		debug(1, "  We listen for node '%s' at %s:%u",
			n->name, inet_ntoa(n->local.sin_addr),
			ntohs(n->local.sin_port));
		debug(1, "  We sent to node '%s' at %s:%u",
			n->name, inet_ntoa(n->remote.sin_addr),
			ntohs(n->remote.sin_port));
	}

	/* Start on thread per path for asynchronous processing */
	for (struct path *p = paths; p; p = p->next) {
		path_start(p);

		info("Starting path: %12s " GRN("=>") " %-12s",
			p->in->name, p->out->name);
	}
}

static void stop()
{
	/* Join all threads and print statistics */
	for (struct path *p = paths; p; p = p->next) {
		path_stop(p);
		path_stats(p);

		info("Stopping path: %12s " RED("=>") " %-12s",
			p->in->name, p->out->name);
	}

	/* Close all sockets we listen on */
	for (struct node *n = nodes; n; n = n->next) {
		node_disconnect(n);
	}

	/* Reset interface queues and affinity */
	for (struct interface *i = interfaces; i; i = i->next) {
		if_setaffinity(i, -1);
		tc_reset(i);
	}
}

static void quit()
{
	stop();

	/** @todo Free nodes and paths */

	config_destroy(&config);

	_exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	/* Check arguments */
	if (argc != 2) {
		printf("Usage: %s CONFIG\n", argv[0]);
		printf("  CONFIG is a required path to a configuration file\n\n");
		printf("Simulator2Simulator Server %s (built on %s, %s)\n", BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
		printf(" Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		printf("   Steffen Vogel <stvogel@eonerc.rwth-aachen.de>\n");
		exit(EXIT_FAILURE);
	}

	info("This is %s %s", BLU("s2ss"), BLU(VERSION));
	debug(1, "Running with debug level: %u", V);

	/* Check priviledges */
	if (getuid() != 0)
		error("The server requires superuser privileges!");

	/* Setup signals */
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT, &sa_quit, NULL);
	atexit(&quit);

	/* Parse configuration file */
	config_init(&config);
	config_parse(argv[1], &config, &settings, &nodes, &paths, &interfaces);

	/* Check for realtime kernel patch */
	struct stat st;
	if (stat("/sys/kernel/realtime", &st))
		warn("Use a RT-preempt patched Linux for lower latencies!");
	else
		debug(3, "This is a realtime patched kernel");

	/* Use FIFO scheduler with realtime priority */
	if (settings.priority) {
		struct sched_param param = { .sched_priority = settings.priority };
		if (sched_setscheduler(0, SCHED_FIFO, &param))
			perror("Failed to set realtime priority");
		else
			debug(3, "Set task priority to %u", settings.priority);
	}

	/* Pin threads to CPUs by setting the affinity */
	if (settings.affinity) {
		cpu_set_t cset = to_cpu_set(settings.affinity);
		if (sched_setaffinity(0, sizeof(cset), &cset))
			perror("Failed to set CPU affinity to '%#x'", settings.affinity);
		else
			debug(3, "Set affinity to %#x", settings.affinity);
	}

	/* Connect all nodes and start one thread per path */
	start();

	if (V >= 5) {
		struct path *p = paths;

		info("");
		info("Runtime Statistics:");
		info("%12s " MAG("=>") " %-12s:   %-8s %-8s %-8s",
			"Source", "Destination", "#Recv", "#Delay", "#Duplicated");
		info("--------------------------------------------------------------");

		while (1) {
			sleep(5);
			path_stats(p);

			p = (p->next) ? p->next : paths;
		}
	}
	else
		pause();

	return 0;
}
