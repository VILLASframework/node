
/** Common entry point for all villas command line tools.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/tool.hpp>

using namespace villas;

void Tool::staticHandler(int signal, siginfo_t *sinfo, void *ctx)
{
	if (current_tool)
		current_tool->handler(signal, sinfo, ctx);
}

void Tool::printCopyright()
{
	std::cout << PROJECT_NAME " " << CLR_BLU(PROJECT_BUILD_ID)
		<< " (built on " CLR_MAG(__DATE__) " " CLR_MAG(__TIME__) ")" << std::endl
		<< " Copyright 2014-2017, Institute for Automation of Complex Power Systems, EONERC" << std::endl
		<< " Steffen Vogel <StVogel@eonerc.rwth-aachen.de>" << std::endl;
}

void Tool::printVersion()
{
	std::cout << PROJECT_BUILD_ID << std::endl;
}

Tool::Tool(int ac, char *av[], const std::string &nme, const std::list<int> &sigs) :
	argc(ac),
	argv(av),
	name(nme),
	handlerSignals(sigs)
{
	current_tool = this;

	logger = logging.get(name);
}

int Tool::run()
{
	try {
		int ret;

		logger->info("This is VILLASnode {} (built on {}, {})",
			CLR_BLD(CLR_YEL(PROJECT_BUILD_ID)),
			CLR_BLD(CLR_MAG(__DATE__)), CLR_BLD(CLR_MAG(__TIME__)));

		ret = utils::signals_init(staticHandler, handlerSignals);
		if (ret)
			throw RuntimeError("Failed to initialize signal subsystem");

		/* Parse command line arguments */
		parse();

		/* Run tool */
		ret = main();

		logger->info(CLR_GRN("Goodbye!"));

		return ret;
	}
	catch (std::runtime_error &e) {
		logger->error("{}", e.what());

		return -1;
	}
}

Tool *Tool::current_tool = nullptr;
