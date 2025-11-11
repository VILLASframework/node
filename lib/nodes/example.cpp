/* An example get started with new implementations of new node-types.
 *
 * This example does not do any particularly useful.
 * It is just a skeleton to get you started with new node-types.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/exceptions.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>
#include <villas/timing.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::utils;
using namespace villas::node;

class ExampleNode : public Node {

protected:
  // Place any configuration and per-node state here

  // Settings
  int setting1;

  std::string setting2;

  // States
  int state1;
  struct timespec start_time;

  int _read(struct Sample *smps[], unsigned cnt) override {
    int read;
    struct timespec now;

    // TODO: Add implementation here. The following is just an example

    assert(cnt >= 1 && smps[0]->capacity >= 1);

    now = time_now();

    smps[0]->data[0].f = time_delta(&now, &start_time);

    /* Dont forget to set other flags in struct Sample::flags
    * E.g. for sequence no, timestamps... */
    smps[0]->flags = (int)SampleFlags::HAS_DATA;
    smps[0]->signals = getInputSignals(false);

    read = 1; // The number of samples read

    return read;
  }

  int _write(struct Sample *smps[], unsigned cnt) override {
    int written;

    // TODO: Add implementation here.

    written = 0; // The number of samples written

    return written;
  }

public:
  ExampleNode(const uuid_t &id = {}, const std::string &name = "")
    : Node(id, name), setting1(72), setting2("something"), state1(0) {}

  /* All of the following virtual-declared functions are optional.
   * Have a look at node.hpp/node.cpp for the default behaviour.
   */

  virtual ~ExampleNode() { }

  int prepare() override {
    state1 = setting1;

    if (setting2 == "double")
      state1 *= 2;

    return 0;
  }

  int parse(json_t *json) override {
    // TODO: Add implementation here. The following is just an example

    const char *setting2_str = nullptr;

    json_error_t err;
    int ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: s }", "setting1",
                            &setting1, "setting2", &setting2_str);
    if (ret)
      throw ConfigError(json, err, "node-config-node-example");

    if (setting2_str)
      setting2 = setting2_str;

    return 0;
  }

  int check() override {
    if (setting1 > 100 || setting1 < 0)
      return -1;

    if (setting2.empty() || setting2.size() > 10)
      return -1;

    return Node::check();
  }

  const std::string &getDetails() override {
    details = fmt::format("setting1={}, setting2={}", setting1, setting2);
    return details;
  }

  int start() override {
    // TODO add implementation here

    start_time = time_now();

    return 0;
  }

  // int ExampleNode::stop() override {
  //   // TODO add implementation here
  //   return 0;
  // }

  // int ExampleNode::pause() override {
  //   // TODO add implementation here
  //   return 0;
  // }

  // int ExampleNode::resume() override {
  //   // TODO add implementation here
  //   return 0;
  // }

  // int ExampleNode::restart() override {
  //   // TODO add implementation here
  //   return 0;
  // }

  // int ExampleNode::reverse() override {
  //   // TODO add implementation here
  //   return 0;
  // }

  // std::vector<int> ExampleNode::getPollFDs() override {
  //   // TODO add implementation here
  //   return {};
  // }

  // std::vector<int> ExampleNode::getNetemFDs() override {
  //   // TODO add implementation here
  //   return {};
  // }

  // struct villas::node::memory::Type * ExampleNode::getMemoryType() override {
  //   // TODO add implementation here
  // }
};

// Register node
static char n[] = "example";
static char d[] = "An example for staring new node-type implementations";
static NodePlugin<ExampleNode, n, d,
                  (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_WRITE |
                      (int)NodeFactory::Flags::SUPPORTS_POLL |
                      (int)NodeFactory::Flags::HIDDEN>
    p;
