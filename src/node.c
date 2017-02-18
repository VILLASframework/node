/** Main routine.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "config.h"

#include <villas/utils.h>
#include <villas/cfg.h>
#include <villas/path.h>
#include <villas/node.h>
#include <villas/api.h>
#include <villas/web.h>
#include <villas/timing.h>
#include <villas/kernel/kernel.h>
#include <villas/kernel/rt.h>

/* Forward declarations */
void hook_stats_header();

#ifdef ENABLE_OPAL_ASYNC
  #include "opal.h"
#endif

struct cfg config;

static void quit()
{
	info("Stopping paths");
	list_foreach(struct path *p, &config.paths) { INDENT
		path_stop(p);
	}

	info("Stopping nodes");
	list_foreach(struct node *n, &config.nodes) { INDENT
		node_stop(n);
	}

	cfg_deinit(&config);
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

static void usage(const char *name)
{
	printf("Usage: %s [CONFIG]\n", name);
	printf("  CONFIG is the path to an optional configuration file\n");
	printf("         if omitted, VILLASnode will start without a configuration\n");
	printf("         and wait for provisioning over the web interface.\n\n");
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
	
	printf("Supported API commands:\n");
	list_foreach(struct api_ressource *r, &apis)
		printf(" - %s: %s\n", r->name, r->description);
	printf("\n");

	print_copyright();

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	/* Check arguments */
#ifdef ENABLE_OPAL_ASYNC
	if (argc != 4)
		usage(argv[0]);
	
	char *uri = "opal-shmem.conf";
#else
	if (argc > 2)
		usage(argv[0]);
	
	char *uri = (argc == 2) ? argv[1] : NULL;
#endif

	info("This is VILLASnode %s (built on %s, %s)", BLD(YEL(VERSION)),
		BLD(MAG(__DATE__)), BLD(MAG(__TIME__)));

	/* Checks system requirements*/
	if (kernel_has_version(KERNEL_VERSION_MAJ, KERNEL_VERSION_MIN))
		error("Your kernel version is to old: required >= %u.%u", KERNEL_VERSION_MAJ, KERNEL_VERSION_MIN);

	info("Initialize signals");
	signals_init();

	info("Parsing configuration");
	cfg_init_pre(&config);
	
	cfg_parse(&config, uri);

	cfg_init_post(&config);

	info("Initialize node types");
	list_foreach(struct node_type *vt, &node_types) { INDENT
		int refs = list_length(&vt->instances);
		if (refs > 0)
			node_init(vt, argc, argv, config_root_setting(config.cfg));
	}

	info("Starting nodes");
	list_foreach(struct node *n, &config.nodes) { INDENT
		int refs = list_count(&config.paths, (cmp_cb_t) path_uses_node, n);
		if (refs > 0)
			node_start(n);
		else
			warn("No path is using the node %s. Skipping...", node_name(n));
	}

	info("Starting paths");
	list_foreach(struct path *p, &config.paths) { INDENT
		if (p->enabled) {
			path_prepare(p);
			path_start(p);
		}
		else
			warn("Path %s is disabled. Skipping...", path_name(p));
	}
	
	if (config.stats > 0)
		hook_stats_header();

	struct timespec now, last = time_now();

	/* Run! Until signal handler is invoked */
	while (1) {
		now = time_now();
		if (config.stats > 0 && time_delta(&last, &now) > config.stats) {
			list_foreach(struct path *p, &config.paths) {
				hook_run(p, NULL, 0, HOOK_PERIODIC);
			}

			last = time_now();
		}

		web_service(&config.web); /** @todo Maybe we should move this to another thread */
	}

	return 0;
}
