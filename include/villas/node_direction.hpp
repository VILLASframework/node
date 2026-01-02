/* Node direction.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>

#include <jansson.h>

#include <villas/common.hpp>
#include <villas/hook_list.hpp>
#include <villas/list.hpp>
#include <villas/signal_list.hpp>

namespace villas {
namespace node {

// Forward declarations
class Node;
class Path;

class NodeDirection {
  friend Node;

public:
  enum class Direction {
    IN, // VILLASnode is receiving/reading
    OUT // VILLASnode is sending/writing
  } direction;

  /* The path which uses this node as a source/destination.
   *
   * Usually every node should be used only by a single path as destination.
   * Otherwise samples from different paths would be interleaved.
   */
  Path *path;
  Node *node;

  int enabled;
  int builtin; // This node should use built-in hooks by default.
  unsigned
      vectorize; // Number of messages to send / recv at once (scatter / gather)

  HookList hooks;          // List of read / write hooks (struct hook).
  SignalList::Ptr signals; // Signal description.

  json_t *config; // A JSON object containing the configuration of the node.

  NodeDirection(enum NodeDirection::Direction dir, Node *n);

  int parse(json_t *json, std::function<Signal::Ptr(json_t *)> parse_signal);
  void check();
  int prepare();
  int start();
  int stop();

  SignalList::Ptr getSignals(int after_hooks = true) const;

  unsigned getSignalsMaxCount() const;
};

} // namespace node
} // namespace villas
