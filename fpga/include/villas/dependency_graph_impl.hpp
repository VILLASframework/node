#ifndef VILLAS_DEPENDENCY_GRAPH_HPP
#error "Do not include this file directly, please include depedency_graph.hpp"
#endif

#include <sstream>
#include "dependency_graph.hpp"

#include "log.hpp"

static auto logger = loggerGetOrCreate("DependencyGraph");

namespace villas {
namespace utils {

template<typename T>
bool
DependencyGraph<T>::addNode(const T &node)
{
	bool existedBefore = nodeExists(node);

	// accessing is enough to create if not exists
	graph[node];

	return existedBefore;
}

template<typename T>
void
DependencyGraph<T>::removeNode(const T &node)
{
	graph.erase(node);

	// check if other nodes depend on this one
	for(auto& [key, dependencies] : graph) {
		if(nodeInList(dependencies, node)) {
			// remove other node that depends on the one to delete
			removeNode(key);
		}
	}

}

template<typename T>
void
DependencyGraph<T>::addDependency(const T &node, const T &dependency)
{
	NodeList& dependencies = graph[node];
	if(not nodeInList(dependencies, dependency))
		dependencies.push_back(dependency);
}

template<typename T>
void
DependencyGraph<T>::dump() {
	for(auto& node : graph) {
		std::stringstream ss;
		for(auto& dep : node.second) {
			ss << dep << " ";
		}
		logger->info("{}: {}", node.first, ss.str());
	}
}

template<typename T>
typename DependencyGraph<T>::NodeList
DependencyGraph<T>::getEvaluationOrder() const
{
	// copy graph to preserve information (we have to delete entries later)
	Graph graph = this->graph;

	// output list
	NodeList out;

	while(graph.size() > 0) {
		int added = 0;

		// look for nodes with no dependencies
		for(auto& [key, dependencies] : graph) {

			for(auto dep = dependencies.begin(); dep != dependencies.end(); ++dep) {
				if(nodeInList(out, *dep)) {
					// dependency has been pushed to list in last round
					dep = dependencies.erase(dep);
				}
			}

			// nodes with no dependencies can be pushed to list
			if(dependencies.empty()) {
				out.push_back(key);
				graph.erase(key);
				added++;
			}
		}

		// if a round doesn't add any elements and is not the last, then
		// there is a circular dependency
		if(added == 0 and graph.size() > 0) {
			logger->error("Circular dependency detected! IPs not available:");
			for(auto& [key, value] : graph) {
				(void) value;
				logger->error("  {}", key);
			}
			break;
		}
	}

	return out;
}

} // namespace utils
} // namespace villas
