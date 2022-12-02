/** Common entry point for all villas command line tools.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
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
		<< " Copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC" << std::endl
		<< " Steffen Vogel <svogel2@eonerc.rwth-aachen.de>" << std::endl;
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

		ret = utils::signalsInit(staticHandler, handlerSignals);
		if (ret)
			throw RuntimeError("Failed to initialize signal subsystem");

		// Parse command line arguments
		parse();

		// Run tool
		ret = main();

		logger->info(CLR_GRN("Goodbye!"));

		return ret;
	} catch (const std::runtime_error &e) {
		logger->error("{}", e.what());

		return -1;
	}
}

Tool *Tool::current_tool = nullptr;
