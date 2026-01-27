/* Node-type for loopback connections.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <jansson.h>

#include <villas/node.hpp>
#include <villas/queue_signalled.h>

namespace villas {
namespace node {

class LoopbackNode : public Node {

protected:
  int queuelen;
  struct CQueueSignalled queue;
  enum QueueSignalledMode mode;

  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;

public:
  LoopbackNode(const uuid_t &id = {}, const std::string &name = "");

  ~LoopbackNode() override;

  int prepare() override;

  int stop() override;

  std::vector<int> getPollFDs() override;

  const std::string &getDetails() override;

  int parse(json_t *json) override;
};

} // namespace node
} // namespace villas
