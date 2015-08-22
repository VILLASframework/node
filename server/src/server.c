/** Main routine.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <sched.h>
#include <signal.h>
#include <unistd.h>

#include "config.h"
#include "utils.h"
#include "cfg.h"
#include "path.h"
#include "node.h"
#include "license.h"

#ifdef ENABLE_OPAL_ASYNC
  #include "opal.h"
#endif

/** Linked list of nodes */
struct list nodes;
/** Linked list of paths */
struct list paths;
/** The global configuration */
struct settings settings;
/** libconfig handle */
static config_t config;

static void quit()
{
	info("Stopping paths");
	FOREACH(&paths, it)
		path_stop(it->path);

	info("Stopping nodes");
	FOREACH(&nodes, it)
		node_stop(it->node);

	node_deinit();

	/* Freeing dynamically allocated memory */
	list_destroy(&paths);
	list_destroy(&nodes);
	config_destroy(&config);

	info("Goodbye!");

	_exit(EXIT_SUCCESS);
}

static void realtime_init()
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
static void signals_init()
{
	struct sigaction sa_quit = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = quit
	};

	sigemptyset(&sa_quit.sa_mask);
	sigaction(SIGINT, &sa_quit, NULL);
	sigaction(SIGTERM, &sa_quit, NULL);
}

static void usage(const char *name)
{
	printf("Usage: %s CONFIG\n", name);
	printf("  CONFIG is a required path to a configuration file\n\n");
#ifdef ENABLE_OPAL_ASYNC
	printf("Usage: %s OPAL_ASYNC_SHMEM_NAME OPAL_ASYNC_SHMEM_SIZE OPAL_PRINT_SHMEM_NAME\n", name);
	printf("  This type of invocation is used by OPAL-RT Asynchronous processes.\n");
	printf("  See in the RT-LAB User Guide for more information.\n\n");
#endif
	printf("Simulator2Simulator Server %s (built on %s %s)\n",
		BLU(VERSION), MAG(__DATE__), MAG(__TIME__));
	printf(" copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC\n");
	printf(" Steffen Vogel <StVogel@eonerc.rwth-aachen.de>\n");

	exit(EXIT_FAILURE);
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

	char *configfile = (argc == 2) ? argv[1] : "opal-shmem.conf";

	log_reset();
	info("This is Simulator2Simulator Server (S2SS) %s (built on %s, %s)",
		BLD(YEL(VERSION)), BLD(MAG(__DATE__)), BLD(MAG(__TIME__)));

	/* Check priviledges */
	if (getuid() != 0)
		error("The server requires superuser privileges!");

	if (check_license())
		error("You're not allowed to use this software!");

	/* Initialize lists */
	list_init(&nodes, (dtor_cb_t) node_destroy);
	list_init(&paths, (dtor_cb_t) path_destroy);

	info("Initialize real-time system");
	realtime_init();

	info("Initialize signals");
	signals_init();

	info("Parsing configuration");
	config_init(&config);
	config_parse(configfile, &config, &settings, &nodes, &paths);

	info("Initialize node types");
	node_init(argc, argv, &settings);

	info("Starting nodes");
	FOREACH(&nodes, it)
		node_start(it->node);

	info("Starting paths");
	FOREACH(&paths, it)
		path_start(it->path);

	/* Run! */
	if (settings.stats > 0) {
		stats_header();

		for (;;) FOREACH(&paths, it) {
			usleep(settings.stats * 1e6);
			hook_run(it->path, HOOK_PERIODIC);
		}
	}
	else
		pause();

	return 0;
}
