/** Main routine.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <iostream>

#include <villas/node/config.h>
#include <villas/copyright.hpp>
#include <villas/log.hpp>
#include <villas/version.hpp>
#include <villas/utils.hpp>
#include <villas/super_node.hpp>
#include <villas/node/exceptions.hpp>

using namespace villas;
using namespace villas::node;

static void usage()
{
	std::cout << "Usage: villas-config-check CONFIG" << std::endl
	          << "  CONFIG is the path to an optional configuration file" << std::endl << std::endl;

	print_copyright();

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	SuperNode sn;
	Logger logger = logging.get("config-test");

	try {
		if (argc != 2)
			usage();

		sn.parse(argv[1]);

		return 0;
	}
	catch (ParseError &e) {
		logger->info("{}", e.what());
	}
	catch (ConfigError &e) {
		logger->info("{}", e.what());
	}
	catch (std::runtime_error &e) {
		logger->info("{}", e.what());
	}


	return -1;
}
