/* Create a graph representation of the VILLASnode configuration file
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>

#include <graphviz/types.h>
#include <graphviz/gvcommon.h>

#include <villas/tool.hpp>
#include <villas/utils.hpp>
#include <villas/super_node.hpp>

using namespace villas;

struct GVC_s {
	GVCOMMON_t common;

	char *config_path;
	bool config_found;

	char **input_filenames; // gvParseArgs
};

namespace villas {
namespace node {
namespace tools {

class Graph : public Tool {

public:
	Graph(int argc, char *argv[]) :
		Tool(argc, argv, "graph", { SIGUSR1, SIGINT }),
		gvc(nullptr),
		graph(nullptr)
	{
		int ret;

		ret = memory::init(DEFAULT_NR_HUGEPAGES);
		if (ret)
			throw RuntimeError("Failed to initialize memory");

		this->argv[0] = (char *) "neato"; // Default layout engine

		gvc = gvContext();
	}

	~Graph()
	{
		gvFreeContext(gvc);
	}

protected:
	GVC_t *gvc;
	graph_t *graph;

	std::string configFilename;

	void usage()
	{
		std::cout << "Usage: villas-graph [OPTIONS]" << std::endl
			  << "For OPTIONS see dot(1).";

		printCopyright();
	}

	void parse()
	{
		gvParseArgs(gvc, argc, argv);

		std::list<std::string> filenames;
		int i;
		for (i = 0; gvc->input_filenames[i]; i++)
			filenames.emplace_back(gvc->input_filenames[i]);

		if (i == 0)
			throw RuntimeError("No configuration file given!");

		configFilename = filenames.front();
	}

	virtual
	void handler(int signal, siginfo_t *siginfp, void *)
	{
#ifndef _WIN32
		switch (signal) {
			case SIGINT:
				// If interrupted we try to produce a partial rendering before exiting
				if (graph)
					gvRenderJobs(gvc, graph);
				break;

			case SIGUSR1:
				// Note that we don't call gvFinalize() so that we don't start event-driven
 	 			// devices like -Tgtk or -Txlib */
				exit(gvFreeContext(gvc));
				break;

			default: { }
		}
#endif // _WIN32
	}

	int main()
	{
		int ret;

		villas::node::SuperNode sn;

		sn.parse(configFilename);
		sn.check();
		sn.prepare();

		graph = sn.getGraph();

		ret = gvLayoutJobs(gvc, graph);  // Take layout engine from command line
		if (ret)
			return ret;

		return gvRenderJobs(gvc, graph);
	}
};

} // namespace tools
} // namespace node
} // namespace villas

int main(int argc, char *argv[])
{
	node::tools::Graph t(argc, argv);

	return t.run();
}
