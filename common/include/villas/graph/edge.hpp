/** A graph edge.
 *
 * @file
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

namespace villas {
namespace graph {

class Edge {
	template<typename VertexType, typename EdgeType>
	friend class DirectedGraph;

public:
	using Identifier = std::size_t;

	friend std::ostream&
	operator<< (std::ostream& stream, const Edge& edge)
	{ return stream << edge.id; }

	bool
	operator==(const Edge& other)
	{ return this->id == other.id; }

	Vertex::Identifier getVertexTo() const
	{ return to; }

	Vertex::Identifier getVertexFrom() const
	{ return from; }

private:
	Identifier id;
	Vertex::Identifier from;
	Vertex::Identifier to;
};

} // namespacae graph
} // namespace villas
