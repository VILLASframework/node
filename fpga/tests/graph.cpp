#include <memory>

#include <criterion/criterion.h>
#include <villas/directed_graph.hpp>

Test(graph, directed, .description = "DirectedGraph")
{
	villas::graph::DirectedGraph<> g;

	std::shared_ptr<villas::graph::Vertex> v1(new villas::graph::Vertex);
	std::shared_ptr<villas::graph::Vertex> v2(new villas::graph::Vertex);
	std::shared_ptr<villas::graph::Vertex> v3(new villas::graph::Vertex);

	auto v1id = g.addVertex(v1);
	auto v2id = g.addVertex(v2);
	auto v3id = g.addVertex(v3);
	cr_assert(g.getVertexCount() == 3);

	g.addEdge(v1id, v2id);
	g.addEdge(v3id, v2id);
	g.addEdge(v1id, v3id);
	g.addEdge(v2id, v1id);
	cr_assert(g.getEdgeCount() == 4);
	cr_assert(g.vertexGetEdges(v1id).size() == 2);
	cr_assert(g.vertexGetEdges(v2id).size() == 1);
	cr_assert(g.vertexGetEdges(v3id).size() == 1);

	g.removeVertex(v1id);
	g.dump();
	cr_assert(g.getVertexCount() == 2);
	cr_assert(g.vertexGetEdges(v2id).size() == 0);
}
