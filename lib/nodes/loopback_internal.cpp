/* Node-type for internal loopback_internal connections.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/colors.hpp>
#include <villas/exceptions.hpp>
#include <villas/node/config.hpp>
#include <villas/node/memory.hpp>
#include <villas/nodes/loopback_internal.hpp>
#include <villas/uuid.hpp>

using namespace villas;
using namespace villas::node;

static InternalLoopbackNodeFactory nf;

InternalLoopbackNode::InternalLoopbackNode(Node *src, unsigned id, unsigned ql)
    : queuelen(ql), source(src) {
  auto name = fmt::format("{}.lo{}", src->getNameShort(), id);

  uuid_t uuid;
  int ret =
      uuid::generateFromString(uuid, fmt::format("lo{}", id), src->getUuid());
  if (ret)
    throw RuntimeError("Failed to initialize UUID");

  Node(uuid, name);

  factory = &nf;
  name_long = fmt::format(CLR_RED("{}") "(" CLR_YEL("{}") ")", name_short,
                          nf.getName());

  auto logger_name = fmt::format("node:{}", getNameShort());

  logger = logging.get(logger_name);

  in.signals = source->getInputSignals(false);

  ret = queue_signalled_init(&queue, queuelen);
  if (ret)
    throw RuntimeError("Failed to initialize queue");

  state = State::PREPARED;
}

InternalLoopbackNode::~InternalLoopbackNode() {
  int ret __attribute__((unused));

  ret = queue_signalled_destroy(&queue);
}

int InternalLoopbackNode::stop() {
  int ret;

  ret = queue_signalled_close(&queue);
  if (ret)
    return ret;

  return 0;
}

int InternalLoopbackNode::_read(struct Sample *smps[], unsigned cnt) {
  int avail;

  struct Sample *cpys[cnt];

  avail = queue_signalled_pull_many(&queue, (void **)cpys, cnt);

  sample_copy_many(smps, cpys, avail);
  sample_decref_many(cpys, avail);

  return avail;
}

int InternalLoopbackNode::_write(struct Sample *smps[], unsigned cnt) {
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

std::vector<int> InternalLoopbackNode::getPollFDs() {
  return {queue_signalled_fd(&queue)};
}
