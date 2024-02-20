/* Path destination.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <villas/queue.h>

namespace villas {
namespace node {

// Forward declarations
class Node;
class Path;
struct Sample;
class PathDestination;

class PathDestination {
  friend Path;

public:
  using Ptr = std::shared_ptr<PathDestination>;

protected:
  Node *node;
  Path *path;

  struct CQueue queue;

public:
  PathDestination(Path *p, Node *n);

  ~PathDestination();

  int prepare(int queuelen);

  void check();

  static void enqueueAll(class Path *p, const struct Sample *const smps[],
                         unsigned cnt);

  void write();

  Node *getNode() const { return node; }
};

using PathDestinationList = std::vector<PathDestination::Ptr>;

} // namespace node
} // namespace villas
