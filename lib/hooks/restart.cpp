/* Path restart hook.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/hook.hpp>
#include <villas/node.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class RestartHook : public Hook {

protected:
  struct Sample *prev;

public:
  using Hook::Hook;

  virtual void start() {
    assert(state == State::PREPARED);

    prev = nullptr;

    state = State::STARTED;
  }

  virtual void stop() {
    assert(state == State::STARTED);

    if (prev)
      sample_decref(prev);

    state = State::STOPPED;
  }

  virtual Hook::Reason process(struct Sample *smp) {
    assert(state == State::STARTED);

    if (prev) {
      // A wrap around of the sequence no should not be treated as a simulation restart
      if (smp->sequence == 0 && prev->sequence != 0 &&
          prev->sequence < UINT64_MAX - 16) {
        logger->warn("Simulation from node {} restarted "
                     "(previous->sequence={}, current->sequence={})",
                     node->getName(), prev->sequence, smp->sequence);

        smp->flags |= (int)SampleFlags::NEW_SIMULATION;

        // Restart hooks
        for (auto k : node->in.hooks)
          k->restart();

        for (auto k : node->out.hooks)
          k->restart();
      }
    }

    sample_incref(smp);
    if (prev)
      sample_decref(prev);

    prev = smp;

    return Reason::OK;
  }
};

// Register hook
static char n[] = "restart";
static char d[] = "Call restart hooks for current node";
static HookPlugin<RestartHook, n, d,
                  (int)Hook::Flags::BUILTIN | (int)Hook::Flags::NODE_READ, 1>
    p;

} // namespace node
} // namespace villas
