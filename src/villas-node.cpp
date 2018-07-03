/** Main routine.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <stdlib.h>
#include <unistd.h>

#include <iostream>

#include <villas/config.h>
#include <villas/utils.h>
#include <villas/super_node.h>
#include <villas/memory.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/api.h>
#include <villas/web.h>
#include <villas/task.h>
#include <villas/plugin.h>
#include <villas/kernel/kernel.h>
#include <villas/kernel/rt.h>
#include <villas/hook.h>
#include <villas/stats.h>
#include <villas/config.h>

#ifdef ENABLE_OPAL_ASYNC
  #include <villas/nodes/opal.h>
#endif

struct super_node sn;

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	int ret;

	if (sn.stats > 0)
		stats_print_footer(STATS_FORMAT_HUMAN);

	ret = super_node_stop(&sn);
	if (ret)
		error("Failed to stop super node");

	ret = super_node_destroy(&sn);
	if (ret)
		error("Failed to destroy super node");

	info(CLR_GRN("Goodbye!"));
	exit(EXIT_SUCCESS);
}

static void usage()
{
	std::cout << "Usage: villas-node [OPTIONS] [CONFIG]" << std::endl;
	std::cout << "  OPTIONS is one or more of the following options:" << std::endl;
	std::cout << "    -h      show this usage information" << std::endl;
	std::cout << "    -V      show the version of the tool" << std::endl << std::endl;
	std::cout << "  CONFIG is the path to an optional configuration file" << std::endl;
	std::cout << "         if omitted, VILLASnode will start without a configuration" << std::endl;
	std::cout << "         and wait for provisioning over the web interface." << std::endl << std::endl;
#ifdef ENABLE_OPAL_ASYNC
	std::cout << "Usage: villas-node OPAL_ASYNC_SHMEM_NAME OPAL_ASYNC_SHMEM_SIZE OPAL_PRINT_SHMEM_NAME" << std::endl;
	std::cout << "  This type of invocation is used by OPAL-RT Asynchronous processes." << std::endl;
	std::cout << "  See in the RT-LAB User Guide for more information." << std::endl << std::endl;
#endif /* ENABLE_OPAL_ASYNC */

	std::cout << "Supported node-types:" << std::endl;
	plugin_dump(PLUGIN_TYPE_NODE);
	std::cout << std::endl;

#ifdef WITH_HOOKS
	std::cout << "Supported hooks:" << std::endl;
	plugin_dump(PLUGIN_TYPE_HOOK);
	std::cout << std::endl;
#endif /* WITH_HOOKS */

	std::cout << "Supported API commands:" << std::endl;
	plugin_dump(PLUGIN_TYPE_API);
	std::cout << std::endl;

	std::cout << "Supported IO formats:" << std::endl;
	plugin_dump(PLUGIN_TYPE_FORMAT);
	std::cout << std::endl;

	print_copyright();

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int ret;

	/* Check arguments */
#ifdef ENABLE_OPAL_ASYNC
	if (argc != 4)
		usage(argv[0]);

	opal_register_region(argc, argv);

	const char *uri = "opal-shmem.conf";
#else
	char c;
	while ((c = getopt(argc, argv, "hV")) != -1) {
		switch (c) {
			case 'V':
				print_version();
				exit(EXIT_SUCCESS);
			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;
	}

	char *uri = argc == optind + 1 ? argv[optind] : NULL;
#endif /* ENABLE_OPAL_ASYNC */

	info("This is VILLASnode %s (built on %s, %s)", CLR_BLD(CLR_YEL(BUILDID)),
		CLR_BLD(CLR_MAG(__DATE__)), CLR_BLD(CLR_MAG(__TIME__)));

#ifdef __linux__
	/* Checks system requirements*/
	struct version kver, reqv = { KERNEL_VERSION_MAJ, KERNEL_VERSION_MIN };
	if (kernel_get_version(&kver) == 0 && version_cmp(&kver, &reqv) < 0)
		error("Your kernel version is to old: required >= %u.%u", KERNEL_VERSION_MAJ, KERNEL_VERSION_MIN);
#endif /* __linux__ */

	ret = signals_init(quit);
	if (ret)
		error("Failed to initialize signal subsystem");

	ret = super_node_init(&sn);
	if (ret)
		error("Failed to initialize super node");

	ret = super_node_parse_uri(&sn, uri);
	if (ret)
		error("Failed to parse command line arguments");

	ret = super_node_check(&sn);
	if (ret)
		error("Failed to verify configuration");

	ret = super_node_start(&sn);
	if (ret)
		error("Failed to start super node");

#ifdef WITH_HOOKS
	if (sn.stats > 0) {
		stats_print_header(STATS_FORMAT_HUMAN);

		struct task t;

		ret = task_init(&t, 1.0 / sn.stats, CLOCK_REALTIME);
		if (ret)
			error("Failed to create stats timer");

		for (;;) {
			task_wait(&t);

			for (size_t i = 0; i < list_length(&sn.paths); i++) {
				struct path *p = (struct path *) list_at(&sn.paths, i);

				if (p->state != STATE_STARTED)
					continue;

				for (size_t j = 0; j < list_length(&p->hooks); j++) {
					struct hook *h = (struct hook *) list_at(&p->hooks, j);

					hook_periodic(h);
				}
			}

			for (size_t i = 0; i < list_length(&sn.nodes); i++) {
				struct node *n = (struct node *) list_at(&sn.nodes, i);

				if (n->state != STATE_STARTED)
					continue;

				for (size_t j = 0; j < list_length(&n->in.hooks); j++) {
					struct hook *h = (struct hook *) list_at(&n->in.hooks, j);

					hook_periodic(h);
				}

				for (size_t j = 0; j < list_length(&n->out.hooks); j++) {
					struct hook *h = (struct hook *) list_at(&n->out.hooks, j);

					hook_periodic(h);
				}
			}
		}
	}
	else
#endif /* WITH_HOOKS */
		for (;;) pause();

	return 0;
}
