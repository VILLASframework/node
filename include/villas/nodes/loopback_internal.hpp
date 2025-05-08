/* Node type: internal loopback.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/node.hpp>
#include <villas/queue_signalled.h>

namespace villas {
namespace node {

class InternalLoopbackNode : public Node {

protected:
  unsigned queuelen;
  struct CQueueSignalled queue;

  Node *source;

  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;

public:
  InternalLoopbackNode(Node *src, unsigned id = 0,
                       unsigned ql = DEFAULT_QUEUE_LENGTH);

  virtual ~InternalLoopbackNode();

  std::vector<int> getPollFDs() override;

  int stop() override;
};

class InternalLoopbackNodeFactory : public NodeFactory {

public:
  using NodeFactory::NodeFactory;

  Node *make(const uuid_t &id = {}, const std::string &name = "") override {
    return nullptr;
  }

  int getFlags() const override {
    return (int)NodeFactory::Flags::INTERNAL |
           (int)NodeFactory::Flags::PROVIDES_SIGNALS |
           (int)NodeFactory::Flags::SUPPORTS_READ |
           (int)NodeFactory::Flags::SUPPORTS_WRITE |
           (int)NodeFactory::Flags::SUPPORTS_POLL;
  }

  std::string getName() const override { return "loopback.internal"; }

  std::string getDescription() const override {
    return "Internal loopback node";
  }
};

} // namespace node
} // namespace villas
