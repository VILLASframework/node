/* An example get started with new implementations of new node-types.
 *
 * This example does not do any particularly useful.
 * It is just a skeleton to get you started with new node-types.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class ExampleNode : public Node {

protected:
  // Place any configuration and per-node state here

  // Settings
  int setting1;

  std::string setting2;

  // States
  int state1;
  struct timespec start_time;

  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;

public:
  ExampleNode(const uuid_t &id = {}, const std::string &name = "");

  /* All of the following virtual-declared functions are optional.
   * Have a look at node.hpp/node.cpp for the default behaviour.
   */

  virtual ~ExampleNode();

  int prepare() override;

  int parse(json_t *json) override;

  int check() override;

  int start() override;

  // int stop() override;

  // int pause() override;

  // int resume() override;

  // int restart() override;

  // int reverse() override;

  // std::vector<int> getPollFDs() override;

  // std::vector<int> getNetemFDs() override;

  // struct villas::node::memory::Type *getMemoryType() override;

  const std::string &getDetails() override;
};

} // namespace node
} // namespace villas
