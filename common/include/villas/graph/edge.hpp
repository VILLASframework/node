/** A graph edge.
 *
 * @file
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

namespace villas {
namespace graph {

class Edge {
	template<typename VertexType, typename EdgeType>
	friend class DirectedGraph;

public:
	using Identifier = std::size_t;

	friend
	std::ostream& operator<< (std::ostream &stream, const Edge &edge)
	{
		return stream << edge.id;
	}

	bool operator==(const Edge &other)
	{
		return this->id == other.id;
	}

	Vertex::Identifier getVertexTo() const
	{
		return to;
	}

	Vertex::Identifier getVertexFrom() const
	{
		return from;
	}

	std::string toString() {
		std::stringstream ss;
		ss << *this;
		return ss.str();
	}

private:
	Identifier id;
	Vertex::Identifier from;
	Vertex::Identifier to;
};

} // namespace graph
} // namespace villas
