/* A graph edge.
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fmt/ostream.h>
#include <villas/config.hpp>
#include <villas/graph/vertex.hpp>

namespace villas {
namespace graph {

class Edge {
  template <typename VertexType, typename EdgeType> friend class DirectedGraph;

public:
  using Identifier = std::size_t;

  friend std::ostream &operator<<(std::ostream &stream, const Edge &edge) {
    return stream << edge.id;
  }

  bool operator==(const Edge &other) { return this->id == other.id; }

  Vertex::Identifier getVertexTo() const { return to; }

  Vertex::Identifier getVertexFrom() const { return from; }

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

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::graph::Edge> : public fmt::ostream_formatter {};
#endif
