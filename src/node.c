/** Main routine.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sched.h>

#include "config.h"

#include <villas/utils.h>
#include <villas/cfg.h>
#include <villas/path.h>
#include <villas/memory.h>
#include <villas/node.h>
#include <villas/kernel/kernel.h>
#include <villas/kernel/rt.h>

#ifdef ENABLE_OPAL_ASYNC
  #include "opal.h"
#endif

struct list paths;		/**< List of paths */
struct list nodes;		/**< List of nodes */

static config_t config;		/**< libconfig handle */
struct settings settings;	/**< The global configuration */

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
	list_destroy(&paths, (dtor_cb_t) path_destroy, false);
	list_destroy(&nodes, (dtor_cb_t) node_destroy, false);
	cfg_destroy(&config);

	info(GRN("Goodbye!"));

	_exit(EXIT_SUCCESS);
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

static void usage()
{
	printf("Usage: villas-node CONFIG\n");
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
	
	printf("Supported hooks:\n");
	list_foreach(struct hook *h, &hooks)
		printf(" - %s: %s\n", h->name, h->description);
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
		usage();

	char *configfile = (argc == 2) ? argv[1] : "opal-shmem.conf";

	log_init();
	info("This is VILLASnode %s (built on %s, %s)", BLD(YEL(VERSION)),
		BLD(MAG(__DATE__)), BLD(MAG(__TIME__)));

	/* Checks system requirements*/
	if (kernel_has_version(KERNEL_VERSION_MAJ, KERNEL_VERSION_MIN))
		error("Your kernel version is to old: required >= %u.%u", KERNEL_VERSION_MAJ, KERNEL_VERSION_MIN);

	/* Initialize lists */
	list_init(&paths);
	list_init(&nodes);

	info("Parsing configuration");
	cfg_parse(configfile, &config, &settings, &nodes, &paths);

	info("Initialize real-time system");
	rt_init(settings.affinity, settings.priority);
	
	info("Initialize memory system");
	memory_init();

	info("Initialize signals");
	signals_init();

	info("Initialize node types");
	list_foreach(struct node_type *vt, &node_types) { INDENT
		int refs = list_length(&vt->instances);
		if (refs > 0)
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
		if (p->enabled) {
			path_init(p);
			
			list_foreach(struct hook *h, &p->hooks)
				hook_init(h, &nodes, &paths, &settings);
			
			path_start(p);
		}
		else
			warn("Path %s is disabled. Skipping...", path_name(p));
	}

	/* Run! */
	if (settings.stats > 0) {
		stats_print_header();

		for (;;) {
			list_foreach(struct path *p, &paths)
				hook_run(p, NULL, 0, HOOK_PERIODIC);
			usleep(settings.stats * 1e6);
		}
	}
	else
		pause();

	return 0;
}
