/** Main routine.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <unistd.h>

#include <sched.h>

#include "config.h"
#include "utils.h"
#include "cfg.h"
#include "path.h"
#include "node.h"
#include "checks.h"

#ifdef ENABLE_OPAL_ASYNC
  #include "opal.h"
#endif

struct list paths;		/**< List of paths */
struct list nodes;		/**< List of nodes */

struct settings settings;	/**< The global configuration */
static config_t config;		/**< libconfig handle */

static void quit()
{
	info("Stopping paths");
	list_foreach(struct path *p, &paths) { INDENT
		path_stop(p);
	}

	info("Stopping nodes");
	list_foreach(struct node *n, &nodes) { INDENT
		node_stop(n);
	}

	info("De-initializing node types");
	list_foreach(struct node_type *vt, &node_types) { INDENT
		node_deinit(vt);
	}

	/* Freeing dynamically allocated memory */
	list_destroy(&paths);
	list_destroy(&nodes);
	config_destroy(&config);

	info(GRN("Goodbye!"));

	_exit(EXIT_SUCCESS);
}

static void realtime_init()
{ INDENT
	if (check_kernel_cmdline())
		warn("You should reserve some cores for the server (see 'isolcpus')");
	if (check_kernel_rt())
		warn("We recommend to use an PREEMPT_RT patched kernel!");
	
	/* Use FIFO scheduler with real time priority */
	if (settings.priority) {
		struct sched_param param = {
			.sched_priority = settings.priority
		};

		if (sched_setscheduler(0, SCHED_FIFO, &param))
			serror("Failed to set real time priority");

		debug(3, "Set task priority to %u", settings.priority);
	}
	warn("Use setting 'priority' to enable real-time scheduling!");

	/* Pin threads to CPUs by setting the affinity */
	if (settings.affinity) {
		cpu_set_t cset = integer_to_cpuset(settings.affinity);
		if (sched_setaffinity(0, sizeof(cset), &cset))
			serror("Failed to set CPU affinity to '%#x'", settings.affinity);

		debug(3, "Set affinity to %#x", settings.affinity);
	}
	warn("Use setting 'affinity' to pin process to isolated CPU cores!");
}

/* Setup exit handler */
static void signals_init()
{ INDENT
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
	printf("Supported node types:\n");
	list_foreach(struct node_type *vt, &node_types)
		printf(" - %s: %s\n", vt->name, vt->description);
	printf("\n");

	print_copyright();

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	/* Check arguments */
#ifdef ENABLE_OPAL_ASYNC
	if (argc != 2 && argc != 4)
#else
	if (argc != 2)
#endif
		usage(argv[0]);

	char *configfile = (argc == 2) ? argv[1] : "opal-shmem.conf";

	log_init();
	info("This is Simulator2Simulator Server (S2SS) %s (built on %s, %s)", BLD(YEL(VERSION)),
		BLD(MAG(__DATE__)), BLD(MAG(__TIME__)));

	/* Checks system requirements*/
	if (check_kernel_version())
		error("Your kernel version is to old: required >= %u.%u", KERNEL_VERSION_MAJ, KERNEL_VERSION_MIN);

	/* Initialize lists */
	list_init(&paths, (dtor_cb_t) path_destroy);
	list_init(&nodes, (dtor_cb_t) node_destroy);

	info("Parsing configuration");
	{ INDENT
		config_init(&config);
		config_parse(configfile, &config, &settings, &nodes, &paths);
	}
	
	info("Initialize real-time system");
	realtime_init();

	info("Initialize signals");
	signals_init();

	info("Initialize hook sub-system");
	hook_init(&nodes, &paths, &settings);

	info("Initialize node types");
	list_foreach(struct node_type *vt, &node_types) { INDENT
		node_init(vt, argc, argv, config_root_setting(&config));
	}

	info("Starting nodes");
	list_foreach(struct node *n, &nodes) { INDENT
		int refs = list_count(&paths, (cmp_cb_t) path_uses_node, n);
		if (refs > 0)
			node_start(n);
		else
			warn("No path is using the node %s. Skipping...", node_name(n));
	}

	info("Starting paths");
	list_foreach(struct path *p, &paths) { INDENT
		if (p->enabled)
			path_start(p);
		else
			warn("Path %s is disabled. Skipping...", path_name(p));
	}

	/* Run! */
	if (settings.stats > 0) {
		hook_stats_header();

		for (;;) {
			list_foreach(struct path *p, &paths)
				hook_run(p, HOOK_PERIODIC);
			usleep(settings.stats * 1e6);
		}
	}
	else
		pause();

	return 0;
}
