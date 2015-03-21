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

#ifdef ENABLE_OPAL_ASYNC
#include "opal.h"
#endif

/** Linked list of nodes */
extern struct list nodes;
/** Linked list of paths */
extern struct list paths;
/** Linked list of interfaces */
extern struct list interfaces;

/** The global configuration */
static struct settings settings;
static config_t config;

static void quit()
{
	info("Stopping paths:");
	FOREACH(&paths, it)
		path_stop(it->path);

	info("Stopping nodes:");
	FOREACH(&nodes, it)
		node_stop(it->node);

	info("Stopping interfaces:");
	FOREACH(&interfaces, it)
		if_stop(it->interface);

#ifdef ENABLE_OPAL_ASYNC
	opal_deinit();
#endif

	/* Freeing dynamically allocated memory */
	list_destroy(&paths);
	list_destroy(&nodes);
	list_destroy(&interfaces);
	config_destroy(&config);

	info("Goodbye!");

	_exit(EXIT_SUCCESS);
}

void realtime_init()
{ INDENT
	/* Use FIFO scheduler with real time priority */
	if (settings.priority) {
		struct sched_param param = { .sched_priority = settings.priority };
		if (sched_setscheduler(0, SCHED_FIFO, &param))
			serror("Failed to set real time priority");
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
}

void usage(const char *name)
{
	printf("Usage: %s CONFIG\n", name);
	printf("  CONFIG is a required path to a configuration file\n\n");
#ifdef ENABLE_OPAL_ASYNC
	printf("Usage: %s OPAL_ASYNC_SHMEM_NAME OPAL_ASYNC_SHMEM_SIZE OPAL_PRINT_SHMEM_NAME\n", name);
	printf("  This type of invocation is used by OPAL-RT Asynchronous processes.\n");
	printf("  See in the RT-LAB User Guide for more information.\n\n");
#endif
	printf("Simulator2Simulator Server %s (built on %s, %s)\n",
		BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
	
	die();
}

int main(int argc, char *argv[])
{
	_mtid = pthread_self();

	/* Check arguments */
#ifdef ENABLE_OPAL_ASYNC
	if (argc != 2 && argc != 4)
#else
	if (argc != 2)
#endif
		usage(argv[0]);

	info("This is Simulator2Simulator Server (S2SS) %s (built on %s, %s, debug=%d)",
		BLD(YEL(VERSION)), BLD(MAG(__DATE__)), BLD(MAG(__TIME__)), _debug);

	/* Check priviledges */
	if (getuid() != 0)
		error("The server requires superuser privileges!");

	/* Initialize lists */
	list_init(&nodes, (dtor_cb_t) node_destroy);
	list_init(&paths, (dtor_cb_t) path_destroy);
	list_init(&interfaces, (dtor_cb_t) if_destroy);


	info("Initialize realtime system:");
	realtime_init();

	info("Setup signals:");
	signals_init();

	info("Parsing configuration:");
	config_init(&config);

#ifdef ENABLE_OPAL_ASYNC
	/* Check if called we are called as an asynchronous process from RT-LAB. */
	opal_init(argc, argv);

	/* @todo: look in predefined locations for a file */
	char *configfile = "opal-shmem.conf";
#else
	char *configfile = argv[1];
#endif

	/* Parse configuration and create nodes/paths */
	config_parse(configfile, &config, &settings, &nodes, &paths);

	/* Connect all nodes and start one thread per path */
	info("Starting nodes:");
	FOREACH(&nodes, it)
		node_start(it->node);

	info("Starting interfaces:");
	FOREACH(&interfaces, it)
		if_start(it->interface, settings.affinity);

	info("Starting paths:");
	FOREACH(&paths, it)
		path_start(it->path);

	/* Run! */
	if (settings.stats > 0) {
		info("Runtime Statistics:");
		info("%-32s :   %-8s %-8s %-8s %-8s %-8s",
			"Source " MAG("=>") " Destination", "#Sent", "#Recv", "#Drop", "#Skip", "#Inval");
		info("---------------------------------------------------------------------------");

		do { FOREACH(&paths, it) {
			usleep(settings.stats * 1e6);
			path_print_stats(it->path);
		} } while (1);

	}
	else
		pause();

	quit();

	return 0;
}
