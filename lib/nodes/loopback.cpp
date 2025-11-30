/* Node-type for loopback connections.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>

#include <villas/exceptions.hpp>
#include <villas/node/config.hpp>
#include <villas/node/memory.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/loopback.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

LoopbackNode::LoopbackNode(const uuid_t &id, const std::string &name)
    : Node(id, name), queuelen(DEFAULT_QUEUE_LENGTH),
      mode(QueueSignalledMode::AUTO) {
  queue.queue.state = State::DESTROYED;
}

LoopbackNode::~LoopbackNode() {
  int ret __attribute__((unused));

  ret = queue_signalled_destroy(&queue);
}

int LoopbackNode::prepare() {
  assert(state == State::CHECKED);

  int ret = queue_signalled_init(&queue, queuelen, &memory::mmap, mode);
  if (ret)
    throw RuntimeError("Failed to initialize queue");

  return Node::prepare();
}

int LoopbackNode::stop() {
  int ret;

  ret = queue_signalled_close(&queue);
  if (ret)
    return ret;

  return 0;
}

int LoopbackNode::_read(struct Sample *smps[], unsigned cnt) {
  int avail;

  struct Sample *cpys[cnt];

  avail = queue_signalled_pull_many(&queue, (void **)cpys, cnt);

  sample_copy_many(smps, cpys, avail);
  sample_decref_many(cpys, avail);

  return avail;
}

int LoopbackNode::_write(struct Sample *smps[], unsigned cnt) {
  sample_incref_many(smps, cnt);

  int pushed = queue_signalled_push_many(&queue, (void **)smps, cnt);
  if (pushed < 0) {
    sample_decref_many(smps, cnt);
    return pushed;
  }

  // Released unpushed samples
  if ((unsigned)pushed < cnt) {
    sample_decref_many(smps + pushed, cnt - pushed);
    logger->warn("Queue overrun");
  }

  return pushed;
}

std::vector<int> LoopbackNode::getPollFDs() {
  return {queue_signalled_fd(&queue)};
}

const std::string &LoopbackNode::getDetails() {
  if (details.empty()) {
    details = fmt::format("queuelen={}", queuelen);
  }

  return details;
}

int LoopbackNode::parse(json_t *json) {
  int ret = Node::parseCommon(json);
  if (ret)
    return ret;

  const char *mode_str = nullptr;

  json_error_t err;
  ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: s }", "queuelen", &queuelen,
                       "mode", &mode_str);
  if (ret)
    throw ConfigError(json, err, "node-config-node-loopback");

  if (mode_str) {
    if (!strcmp(mode_str, "auto"))
      mode = QueueSignalledMode::AUTO;
#ifdef HAVE_EVENTFD
    else if (!strcmp(mode_str, "eventfd"))
      mode = QueueSignalledMode::EVENTFD;
#endif
    else if (!strcmp(mode_str, "pthread"))
      mode = QueueSignalledMode::PTHREAD;
    else if (!strcmp(mode_str, "polling"))
      mode = QueueSignalledMode::POLLING;
    else
      throw ConfigError(json, "node-config-node-loopback-mode",
                        "Unknown mode '{}'", mode_str);
  }

  return 0;
}

// Register node
static char n[] = "loopback";
static char d[] = "Loopback node-type";
static NodePlugin<LoopbackNode, n, d,
                  (int)NodeFactory::Flags::SUPPORTS_POLL |
                      (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_WRITE>
    nf;
