/* Node direction.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/config.hpp>
#include <villas/exceptions.hpp>
#include <villas/hook.hpp>
#include <villas/hook_list.hpp>
#include <villas/node.hpp>
#include <villas/node_compat_type.hpp>
#include <villas/node_direction.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

NodeDirection::NodeDirection(enum NodeDirection::Direction dir, Node *n)
    : direction(dir), path(nullptr), node(n), enabled(1), builtin(1),
      vectorize(1), config(nullptr) {}

int NodeDirection::parse(json_t *json) {
  int ret;

  json_error_t err;
  json_t *json_hooks = nullptr;
  json_t *json_signals = nullptr;

  config = json;

  ret = json_unpack_ex(json, &err, 0, "{ s?: o, s?: o, s?: i, s?: b, s?: b }",
                       "hooks", &json_hooks, "signals", &json_signals,
                       "vectorize", &vectorize, "builtin", &builtin, "enabled",
                       &enabled);
  if (ret)
    throw ConfigError(json, err, "node-config-node-in");

  if (node->getFactory()->getFlags() &
      (int)NodeFactory::Flags::PROVIDES_SIGNALS) {
    // Do nothing.. Node-type will provide signals
    signals = std::make_shared<SignalList>();
    if (!signals)
      throw MemoryAllocationError();
  } else if (json_signals) {
    signals = std::make_shared<SignalList>(json_signals);
    if (!signals)
      throw MemoryAllocationError();
  } else {
    signals =
        std::make_shared<SignalList>(DEFAULT_SAMPLE_LENGTH, SignalType::FLOAT);
    if (!signals)
      return -1;
  }

#ifdef WITH_HOOKS
  if (json_hooks) {
    int m = direction == NodeDirection::Direction::OUT
                ? (int)Hook::Flags::NODE_WRITE
                : (int)Hook::Flags::NODE_READ;

    hooks.parse(json_hooks, m, nullptr, node);
  }
#endif // WITH_HOOKS

  return 0;
}

void NodeDirection::check() {
  if (vectorize <= 0)
    throw RuntimeError(
        "Invalid setting 'vectorize' with value {}. Must be natural number!",
        vectorize);

#ifdef WITH_HOOKS
  hooks.check();
#endif // WITH_HOOKS
}

int NodeDirection::prepare() {
#ifdef WITH_HOOKS
  int t = direction == NodeDirection::Direction::OUT
              ? (int)Hook::Flags::NODE_WRITE
              : (int)Hook::Flags::NODE_READ;
  int m = builtin ? t | (int)Hook::Flags::BUILTIN : 0;

  hooks.prepare(signals, m, nullptr, node);
#endif // WITH_HOOKS

  return 0;
}

int NodeDirection::start() {
#ifdef WITH_HOOKS
  hooks.start();
#endif // WITH_HOOKS

  return 0;
}

int NodeDirection::stop() {
#ifdef WITH_HOOKS
  hooks.stop();
#endif // WITH_HOOKS

  return 0;
}

SignalList::Ptr NodeDirection::getSignals(int after_hooks) const {
#ifdef WITH_HOOKS
  if (after_hooks && hooks.size() > 0)
    return hooks.getSignals();
#endif // WITH_HOOKS

  return signals;
}

unsigned NodeDirection::getSignalsMaxCount() const {
#ifdef WITH_HOOKS
  if (hooks.size() > 0)
    return MAX(signals->size(), hooks.getSignalsMaxCount());
#endif // WITH_HOOKS

  return signals->size();
}
