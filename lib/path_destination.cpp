/* Path destination.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/exceptions.hpp>
#include <villas/node.hpp>
#include <villas/node/memory.hpp>
#include <villas/path.hpp>
#include <villas/path_destination.hpp>
#include <villas/sample.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;

PathDestination::PathDestination(Path *p, Node *n) : node(n), path(p) {
  queue.state = State::DESTROYED;
}

PathDestination::~PathDestination() {
  int ret __attribute__((unused));

  ret = queue_destroy(&queue);
}

int PathDestination::prepare(int queuelen) {
  int ret;

  ret = queue_init(&queue, queuelen);
  if (ret)
    return ret;

  return 0;
}

void PathDestination::enqueueAll(Path *p, const struct Sample *const smps[],
                                 unsigned cnt) {
  unsigned enqueued, cloned;

  struct Sample *clones[cnt];

  cloned = sample_clone_many(clones, smps, cnt);
  if (cloned < cnt)
    p->logger->warn("Pool underrun in path {}", p->toString());

  for (auto pd : p->destinations) {
    enqueued = queue_push_many(&pd->queue, (void **)clones, cloned);
    if (enqueued != cnt)
      p->logger->warn("Queue overrun for path {}", p->toString());

    // Increase reference counter of these samples as they are now also owned by the queue
    sample_incref_many(clones, cloned);

    p->logger->debug("Enqueued {} samples to destination {} of path {}",
                     enqueued, pd->node->getName(), p->toString());
  }

  sample_decref_many(clones, cloned);
}

void PathDestination::write() {
  int cnt = node->out.vectorize;
  int sent;
  int allocated;

  struct Sample *smps[cnt];

  // As long as there are still samples in the queue
  while (true) {
    allocated = queue_pull_many(&queue, (void **)smps, cnt);
    if (allocated == 0)
      break;
    else if (allocated < cnt)
      path->logger->debug(
          "Queue underrun for path {}: allocated={} expected={}",
          path->toString(), allocated, cnt);

    path->logger->debug(
        "Dequeued {} samples from queue of node {} which is part of path {}",
        allocated, node->getName(), path->toString());

    sent = node->write(smps, allocated);
    if (sent < 0) {
      path->logger->error("Failed to sent {} samples to node {}: reason={}",
                          cnt, node->getName(), sent);
      return;
    } else if (sent < allocated)
      path->logger->debug("Partial write to node {}: written={}, expected={}",
                          node->getName(), sent, allocated);

    int released = sample_decref_many(smps, allocated);

    path->logger->debug("Released {} samples back to memory pool", released);
  }
}

void PathDestination::check() {
  if (!node->isEnabled())
    throw RuntimeError("Destination {} is not enabled", node->getName());

  if (!(node->getFactory()->getFlags() &
        (int)NodeFactory::Flags::SUPPORTS_WRITE))
    throw RuntimeError(
        "Destination node {} is not supported as a sink for path ",
        node->getName());
}
