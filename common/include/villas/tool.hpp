/** Common entry point for all villas command line tools.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <villas/log.hpp>
#include <villas/colors.hpp>
#include <villas/config.hpp>
#include <villas/exceptions.hpp>
#include <villas/utils.hpp>

namespace villas {

class Tool {

protected:
	Logger logger;

	int argc;
	char **argv;

	std::string name;

	static Tool *current_tool;

	static void staticHandler(int signal, siginfo_t *sinfo, void *ctx);

	virtual void handler(int, siginfo_t *, void *)
	{ }

	std::list<int> handlerSignals;

	static void printCopyright();
	static void printVersion();

public:
	Tool(int ac, char *av[], const std::string &name, const std::list<int> &sigs = { });

	virtual int main()
	{
		return 0;
	}

	virtual void usage()
	{ }

	virtual void parse()
	{ }

	int run();
};

} /* namespace villas */
