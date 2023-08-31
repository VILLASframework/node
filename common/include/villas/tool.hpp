/* Common entry point for all villas command line tools.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

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

	static
	Tool *current_tool;

	static
	void staticHandler(int signal, siginfo_t *sinfo, void *ctx);

	virtual
	void handler(int, siginfo_t *, void *)
	{ }

	std::list<int> handlerSignals;

	static
	void printCopyright();

	static
	void printVersion();

public:
	Tool(int ac, char *av[], const std::string &name, const std::list<int> &sigs = { });

	virtual
	int main()
	{
		return 0;
	}

	virtual
	void usage()
	{ }

	virtual
	void parse()
	{ }

	virtual
	int run();
};

} // namespace villas
