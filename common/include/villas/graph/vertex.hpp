/** A graph vertex.
 *
 * @file
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

namespace villas {
namespace graph {

class Vertex {
	template<typename VertexType, typename EdgeType>
	friend class DirectedGraph;

public:
	using Identifier = std::size_t;

	Vertex() :
		id(0)
	{ }

	const Identifier& getIdentifier() const
	{
		return id;
	}

	friend
	std::ostream& operator<<(std::ostream &stream, const Vertex &vertex)
	{
		return stream << vertex.id;
	}

	bool operator==(const Vertex &other)
	{
		return this->id == other.id;
	}

	std::string toString() {
		std::stringstream ss;
		ss << *this;
		return ss.str();
	}

private:
	Identifier id;
	// HACK: how to resolve this circular type dependency?
	std::list<std::size_t> edges;
};

} // namespace graph
} // namespace villas
