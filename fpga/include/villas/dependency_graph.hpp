#ifndef VILLAS_DEPENDENCY_GRAPH_HPP
#define VILLAS_DEPENDENCY_GRAPH_HPP

#include <list>
#include <map>
#include <algorithm>

#include "log.hpp"

namespace villas {
namespace utils {


template<typename T>
class DependencyGraph {
public:
	using NodeList = std::list<T>;

	/// Create a node without dependencies if it not yet exists, return if a new
	/// node has been created.
	bool addNode(const T& node);

	/// Remove a node and all other nodes that depend on it
	void removeNode(const T& node);

	/// Add a dependency to a node. Will create the node if it not yet exists
	void addDependency(const T& node, const T& dependency);

	void dump();

	/// Return a sequential evaluation order list. If a circular dependency has been
	/// detected, all nodes involved will not be part of that list.
	NodeList getEvaluationOrder() const;

private:
	/// Return whether a node already exists or not
	bool nodeExists(const T& node)
	{ return graph.find(node) != graph.end(); }

	static bool
	nodeInList(const NodeList& list, const T& node)
	{ return list.end() != std::find(list.begin(), list.end(), node); }

private:
	using Graph = std::map<T, NodeList>;

	Graph graph;
};

} // namespace utils
} // namespace villas

#include "dependency_graph_impl.hpp"

#endif // VILLAS_DEPENDENCY_GRAPH_HPP
