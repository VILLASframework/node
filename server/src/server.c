/** Main routine.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <stdio.h>

#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <netinet/ip.h>

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
	/* Connect and bind nodes to their sockets, set socket options */
	for (struct node *n = nodes; n; n = n->next) {
		/* Determine outgoing interface */
		int index = if_getegress(&n->remote);
		if (index < 0)
			error("Failed to get egress interface for node '%s'", n->name);

		n->interface = if_lookup_index(index, interfaces);

		/* Create new interface */
		if (!n->interface) {
			struct interface *i = malloc(sizeof(struct interface));
			if (!i)
				error("Failed to allocate memory for interface");
			else
				memset(i, 0, sizeof(struct interface));

			i->index = index;
			if_indextoname(index, i->name);

			debug(3, "Setup interface '%s'", i->name,
				i->index, i->refcnt);

			/* Set affinity for network interfaces */
			if (settings.affinity && i->index) {
				if_getirqs(i);
				if_setaffinity(i, settings.affinity);
			}

			list_add(interfaces, i);
			n->interface = i;
		}

		node_connect(n);

		/* Set fwmark for outgoing packets */
		if (n->netem) {
			n->mark = 1 + n->interface->refcnt++;

			if (setsockopt(n->sd, SOL_SOCKET, SO_MARK, &n->mark, sizeof(n->mark)))
				perror("Failed to set fwmark for outgoing packets");
			else
				debug(4, "Set fwmark of outgoing packets of node '%s' to %u",
					n->name, n->mark);
		}

#if 0		/* Set QoS or TOS IP options */
		int prio = SOCKET_PRIO;	
		if (setsockopt(n->sd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
			perror("Failed to set socket priority");
		else
			debug(4, "Set socket priority for node '%s' to %u", n->name, prio);
#else
		int tos = IPTOS_LOWDELAY;
		if (setsockopt(n->sd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)))
			perror("Failed to set type of service (QoS)");
		else
			debug(4, "Set QoS/TOS IP option for node '%s' to %#x", n->name, tos);
#endif
	}

	/* Setup network emulation */
	for (struct interface *i = interfaces; i; i = i->next) {
		if (i->refcnt)
			tc_prio(i, TC_HDL(4000, 0), i->refcnt);
	}

	for (struct node *n = nodes; n; n = n->next) {
		if (n->netem) {
			tc_mark(n->interface,  TC_HDL(4000, n->mark), n->mark);
			tc_netem(n->interface, TC_HDL(4000, n->mark), n->netem);
		}
	}

	/* Start on thread per path for asynchronous processing */
	for (struct path *p = paths; p; p = p->next) {
		path_start(p);

		info("Path started: %12s " GRN("=>") " %-12s",
			p->in->name, p->out->name);
	}
}

static void stop()
{
	/* Join all threads and print statistics */
	for (struct path *p = paths; p; p = p->next) {
		path_stop(p);

		info("Path stopped: %12s " RED("=>") " %-12s",
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
		printf("Simulator2Simulator Server %s (built on %s, %s)\n",
			BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
		printf(" Copyright 2014, Institute for Automation of Complex Power Systems, EONERC\n");
		printf("   Steffen Vogel <stvogel@eonerc.rwth-aachen.de>\n");
		exit(EXIT_FAILURE);
	}

	info("This is %s %s", BLU("s2ss"), BLU(VERSION));

	/* Setup exit handler */
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
	config_parse(argv[1], &config, &settings, &nodes, &paths);

	debug(1, "Running with debug level: %u", settings.debug);

	/* Check priviledges */
	if (getuid() != 0)
		error("The server requires superuser privileges!");

	/* Check for realtime kernel patch */
	struct stat st;
	if (stat("/sys/kernel/realtime", &st))
		warn("Use a RT-preempt patched Linux for lower latencies!");
	else
		info("Server is running on a realtime patched kernel");

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

	if (settings.stats > 0) {
		struct path *p = paths;

		info("Runtime Statistics:");
		info("%12s " MAG("=>") " %-12s:   %-8s %-8s %-8s %-8s %-8s",
			"Source", "Destination", "#Sent", "#Recv", "#Drop", "#Skip", "#Inval");
		info("---------------------------------------------------------------------------");

		while (1) {
			usleep(settings.stats * 1e6);
			path_stats(p);

			p = (p->next) ? p->next : paths; 
		}
	}
	else
		pause();

	return 0;
}
