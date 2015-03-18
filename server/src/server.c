/** Main routine.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
extern struct node *nodes;
/** Linked list of paths */
extern struct path *paths;
/** Linked list of interfaces */
extern struct interface *interfaces;

/** The global configuration */
struct settings settings;
config_t config;

static void quit()
{ _indent = 0;
	info("Stopping paths:");
	for (struct path *p = paths; p; p = p->next) { INDENT
		path_stop(p);
		path_destroy(p);
	}

	info("Stopping nodes:");
	for (struct node *n = nodes; n; n = n->next) { INDENT
		node_stop(n);
	}

	info("Stopping interfaces:");
	for (struct interface *i = interfaces; i; i = i->next) { INDENT
		if_stop(i);
	}

	/** @todo Free nodes */

	config_destroy(&config);

	_exit(EXIT_SUCCESS);
}

void realtime_init()
{ INDENT
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
			serror("Failed to set realtime priority");
		else
			debug(3, "Set task priority to %u", settings.priority);
	}

	/* Pin threads to CPUs by setting the affinity */
	if (settings.affinity) {
		cpu_set_t cset = to_cpu_set(settings.affinity);
		if (sched_setaffinity(0, sizeof(cset), &cset))
			serror("Failed to set CPU affinity to '%#x'", settings.affinity);
		else
			debug(3, "Set affinity to %#x", settings.affinity);
	}
}

/* Setup exit handler */
void signals_init()
{ INDENT
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGTERM, &sa_quit, NULL);
	sigaction(SIGINT, &sa_quit, NULL);
	atexit(&quit);
}

void usage(const char *name)
{
	printf("Usage: %s CONFIG\n", name);
	printf("  CONFIG is a required path to a configuration file\n\n");
	printf("Simulator2Simulator Server %s (built on %s, %s)\n",
		BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
	
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	/* Check arguments */
	if (argc != 2)
		usage(argv[0]);
	
	epoch_reset();
	info("This is Simulator2Simulator Server (S2SS) %s (built on %s, %s)",
		BLD(YEL(VERSION)), BLD(MAG(__DATE__)), BLD(MAG(__TIME__)));

	/* Check priviledges */
	if (getuid() != 0)
		error("The server requires superuser privileges!");

	/* Start initialization */
	info("Initialize realtime system:");
	realtime_init();
	info("Setup signals:");
	signals_init();
	info("Parsing configuration:");
	config_init(&config);

	/* Parse configuration and create nodes/paths */
	config_parse(argv[1], &config, &settings, &nodes, &paths);

	/* Connect all nodes and start one thread per path */
	info("Starting nodes:");
	for (struct node *n = nodes; n; n = n->next) { INDENT
		node_start(n);
	}

	info("Starting interfaces:");
	for (struct interface *i = interfaces; i; i = i->next) { INDENT
		if_start(i, settings.affinity);
	}

	info("Starting pathes:");
	for (struct path *p = paths; p; p = p->next) { INDENT
		path_start(p);
	}

	/* Run! */
	if (settings.stats > 0) {
		struct path *p = paths;

		info("Runtime Statistics:");
		info("%-32s :   %-8s %-8s %-8s %-8s %-8s",
			"Source " MAG("=>") " Destination", "#Sent", "#Recv", "#Drop", "#Skip", "#Inval");
		info("---------------------------------------------------------------------------");

		while (1) {
			usleep(settings.stats * 1e6);
			path_stats(p);

			p = (p->next) ? p->next : paths; 
		}
	}
	else
		pause();

	/* Note: quit() is called by exit handler! */

	return 0;
}
