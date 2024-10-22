/* Drop hook.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/hook.hpp>
#include <villas/node.hpp>
#include <villas/sample.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

class FixHook : public Hook {

public:
  using Hook::Hook;

  virtual Hook::Reason process(struct Sample *smp) {
    assert(state == State::STARTED);

    timespec now = time_now();

    if (!(smp->flags & (int)SampleFlags::HAS_SEQUENCE) && node) {
      smp->sequence = node->sequence++;
      smp->flags |= (int)SampleFlags::HAS_SEQUENCE;
    }

    if (!(smp->flags & (int)SampleFlags::HAS_TS_RECEIVED)) {
      smp->ts.received = now;
      smp->flags |= (int)SampleFlags::HAS_TS_RECEIVED;
    }

    if (!(smp->flags & (int)SampleFlags::HAS_TS_ORIGIN)) {
      smp->ts.origin = smp->ts.received;
      smp->flags |= (int)SampleFlags::HAS_TS_ORIGIN;
    }

    return Reason::OK;
  }
};

// Register hook
static char n[] = "fix";
static char d[] = "Fix received data by adding missing fields";
static HookPlugin<FixHook, n, d,
                  (int)Hook::Flags::BUILTIN | (int)Hook::Flags::NODE_READ, 1>
    p;

} // namespace node
} // namespace villas
