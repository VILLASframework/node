/** Graph unit test.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Steffen Vogel
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
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

#include <memory>

#include <criterion/criterion.h>
#include <villas/graph/directed.hpp>
#include <villas/log.hpp>
#include <villas/memory_manager.hpp>

static void init_graph()
{
	spdlog::set_pattern("[%T] [%l] [%n] %v");
	spdlog::set_level(spdlog::level::debug);
}

TestSuite(graph,
    .description = "Graph library",
    .init = init_graph
);

Test(graph, basic, .description = "DirectedGraph")
{
	auto logger = loggerGetOrCreate("test:graph:basic");
	villas::graph::DirectedGraph<> g("test:graph:basic");

	logger->info("Testing basic graph construction and modification");

	std::shared_ptr<villas::graph::Vertex> v1(new villas::graph::Vertex);
	std::shared_ptr<villas::graph::Vertex> v2(new villas::graph::Vertex);
	std::shared_ptr<villas::graph::Vertex> v3(new villas::graph::Vertex);

	auto v1id = g.addVertex(v1);
	auto v2id = g.addVertex(v2);
	auto v3id = g.addVertex(v3);
	cr_assert(g.getVertexCount() == 3);

	g.addDefaultEdge(v1id, v2id);
	g.addDefaultEdge(v3id, v2id);
	g.addDefaultEdge(v1id, v3id);
	g.addDefaultEdge(v2id, v1id);
	cr_assert(g.getEdgeCount() == 4);
	cr_assert(g.vertexGetEdges(v1id).size() == 2);
	cr_assert(g.vertexGetEdges(v2id).size() == 1);
	cr_assert(g.vertexGetEdges(v3id).size() == 1);

	g.removeVertex(v1id);
	g.dump();
	cr_assert(g.getVertexCount() == 2);
	cr_assert(g.vertexGetEdges(v2id).size() == 0);

	logger->info(TXT_GREEN("Passed"));
}

Test(graph, path, .description = "Find path")
{
	auto logger = loggerGetOrCreate("test:graph:path");
	logger->info("Testing path finding algorithm");

	using Graph = villas::graph::DirectedGraph<>;
	Graph g("test:graph:path");

	std::shared_ptr<villas::graph::Vertex> v1(new villas::graph::Vertex);
	std::shared_ptr<villas::graph::Vertex> v2(new villas::graph::Vertex);
	std::shared_ptr<villas::graph::Vertex> v3(new villas::graph::Vertex);
	std::shared_ptr<villas::graph::Vertex> v4(new villas::graph::Vertex);
	std::shared_ptr<villas::graph::Vertex> v5(new villas::graph::Vertex);
	std::shared_ptr<villas::graph::Vertex> v6(new villas::graph::Vertex);

	auto v1id = g.addVertex(v1);
	auto v2id = g.addVertex(v2);
	auto v3id = g.addVertex(v3);

	auto v4id = g.addVertex(v4);
	auto v5id = g.addVertex(v5);
	auto v6id = g.addVertex(v6);

	g.addDefaultEdge(v1id, v2id);
	g.addDefaultEdge(v2id, v3id);

	// create circular subgraph
	g.addDefaultEdge(v4id, v5id);
	g.addDefaultEdge(v5id, v4id);
	g.addDefaultEdge(v5id, v6id);

	g.dump();

	logger->info("Find simple path via two edges");
	std::list<Graph::EdgeIdentifier> path1;
	cr_assert(g.getPath(v1id, v3id, path1));

	logger->info("  Path from {} to {} via:", v1id, v3id);
	for(auto& edge : path1) {
		logger->info("  -> edge {}", edge);
	}

	logger->info("Find path between two unconnected sub-graphs");
	std::list<Graph::EdgeIdentifier> path2;
	cr_assert(not g.getPath(v1id, v4id, path2));
	logger->info("  no path found -> ok");


	logger->info("Find non-existing path in circular sub-graph");
	std::list<Graph::EdgeIdentifier> path3;
	cr_assert(not g.getPath(v4id, v2id, path3));
	logger->info("  no path found -> ok");


	logger->info("Find path in circular graph");
	std::list<Graph::EdgeIdentifier> path4;
	cr_assert(g.getPath(v4id, v6id, path4));

	logger->info("  Path from {} to {} via:", v4id, v6id);
	for(auto& edge : path4) {
		logger->info("  -> edge {}", edge);
	}

	logger->info(TXT_GREEN("Passed"));
}

Test(graph, memory_manager, .description = "Global Memory Manager")
{
	auto logger = loggerGetOrCreate("test:graph:mm");
	auto& mm = villas::MemoryManager::get();

	logger->info("Create address spaces");
	auto dmaRegs = mm.getOrCreateAddressSpace("DMA Registers");
	auto pcieBridge = mm.getOrCreateAddressSpace("PCIe Bridge");

	logger->info("Create a mapping");
	mm.createMapping(0x1000, 0, 0x1000, "Testmapping", dmaRegs, pcieBridge);

	logger->info("Find address space by name");
	auto vertex = mm.findAddressSpace("PCIe Bridge");
	logger->info("  found: {}", vertex);

	mm.dump();

	logger->info(TXT_GREEN("Passed"));
}
