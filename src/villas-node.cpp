/** Main routine.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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
#include <exception>
#include <atomic>

#include <villas/node/config.h>
#include <villas/version.hpp>
#include <villas/utils.hpp>
#include <villas/super_node.hpp>
#include <villas/plugin.hpp>
#include <villas/api/action.hpp>
#include <villas/memory.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/api.hpp>
#include <villas/web.hpp>
#include <villas/log.hpp>
#include <villas/exceptions.hpp>
#include <villas/copyright.hpp>
#include <villas/plugin.h>
#include <villas/kernel/kernel.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/hook.h>
#include <villas/stats.h>

#ifdef ENABLE_OPAL_ASYNC
  #include <villas/nodes/opal.h>
#endif

using namespace villas;
using namespace villas::node;
using namespace villas::plugin;

static std::atomic<bool> stop(false);

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	stop = true;
}

static void usage()
{
	std::cout << "Usage: villas-node [OPTIONS] [CONFIG]" << std::endl
	          << "  OPTIONS is one or more of the following options:" << std::endl
	          << "    -h      show this usage information" << std::endl
	          << "    -d LVL  set logging level" << std::endl
	          << "    -V      show the version of the tool" << std::endl << std::endl
	          << "  CONFIG is the path to an optional configuration file" << std::endl
	          << "         if omitted, VILLASnode will start without a configuration" << std::endl
	          << "         and wait for provisioning over the web interface." << std::endl << std::endl
#ifdef ENABLE_OPAL_ASYNC
	          << "Usage: villas-node OPAL_ASYNC_SHMEM_NAME OPAL_ASYNC_SHMEM_SIZE OPAL_PRINT_SHMEM_NAME" << std::endl
	          << "  This type of invocation is used by OPAL-RT Asynchronous processes." << std::endl
	          << "  See in the RT-LAB User Guide for more information." << std::endl << std::endl
#endif /* ENABLE_OPAL_ASYNC */

	          << "Supported node-types:" << std::endl;
	plugin_dump(PLUGIN_TYPE_NODE);
	std::cout << std::endl;

#ifdef WITH_HOOKS
	std::cout << "Supported hooks:" << std::endl;
	plugin_dump(PLUGIN_TYPE_HOOK);
	std::cout << std::endl;
#endif /* WITH_HOOKS */

#ifdef WITH_API
	std::cout << "Supported API commands:" << std::endl;
	for (Plugin *p : Registry::lookup<api::ActionFactory>())
		std::cout << " - " << p->getName() << ": " << p->getDescription() << std::endl;
	std::cout << std::endl;
#endif /* WITH_API */

	std::cout << "Supported IO formats:" << std::endl;
	plugin_dump(PLUGIN_TYPE_FORMAT);
	std::cout << std::endl;

	print_copyright();

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int ret;

	SuperNode sn;
	Logger logger = logging.get("node");

	try {

	/* Check arguments */
#ifdef ENABLE_OPAL_ASYNC
	if (argc != 4)
		usage(argv[0]);

	opal_register_region(argc, argv);

	const char *uri = "opal-shmem.conf";
#else

	/* Parse optional command line arguments */
	int c;
	while ((c = getopt(argc, argv, "hVd:")) != -1) {
		switch (c) {
			case 'V':
				print_version();
				exit(EXIT_SUCCESS);

			case 'd':
				logging.setLevel(optarg);
				break;

			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;
	}

	char *uri = argc == optind + 1 ? argv[optind] : nullptr;
#endif /* ENABLE_OPAL_ASYNC */

	logger->info("This is VILLASnode {} (built on {}, {})",
		CLR_BLD(CLR_YEL(PROJECT_BUILD_ID)),
		CLR_BLD(CLR_MAG(__DATE__)), CLR_BLD(CLR_MAG(__TIME__)));

#ifdef __linux__
	/* Checks system requirements*/
	auto required = utils::Version(KERNEL_VERSION_MAJ, KERNEL_VERSION_MIN);
	if (kernel::getVersion() < required)
		throw RuntimeError("Your kernel version is to old: required >= {}.{}", KERNEL_VERSION_MAJ, KERNEL_VERSION_MIN);
#endif /* __linux__ */

	ret = utils::signals_init(quit);
	if (ret)
		throw RuntimeError("Failed to initialize signal subsystem");

	if (uri) {
		ret = sn.parseUri(uri);
		if (ret)
			throw RuntimeError("Failed to parse command line arguments");
	}
	else
		logger->warn("No configuration file specified. Starting unconfigured. Use the API to configure this instance.");

	ret = sn.check();
	if (ret)
		throw RuntimeError("Failed to verify configuration");

	ret = sn.start();
	if (ret)
		throw RuntimeError("Failed to start super node");

	while (!stop)
		sn.run();

	ret = sn.stop();
	if (ret)
		throw RuntimeError("Failed to stop super node");

	logger->info(CLR_GRN("Goodbye!"));

	return 0;

	}
	catch (std::exception *e) {
		logger->error(e->what());
	}
}
