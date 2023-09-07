/* Limit hook.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bitset>

#include <cstring>

#include <villas/hook.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>

namespace villas {
namespace node {

class LimitValueHook : public MultiSignalHook {

protected:
  unsigned offset;

  float min, max;

public:
  LimitValueHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : MultiSignalHook(p, n, fl, prio, en), offset(0), min(0), max(0) {}

  virtual void parse(json_t *json) {
    int ret;
    json_error_t err;

    assert(state != State::STARTED);

    MultiSignalHook::parse(json);

    ret = json_unpack_ex(json, &err, 0, "{ s: f, s: f }", "min", &min, "max",
                         &max);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-average");

    state = State::PARSED;
  }

  virtual Hook::Reason process(struct Sample *smp) {
    assert(state == State::STARTED);

    for (auto index : signalIndices) {
      switch (sample_format(smp, index)) {
      case SignalType::INTEGER:
        if (smp->data[index].i > max)
          smp->data[index].i = max;

        if (smp->data[index].i < min)
          smp->data[index].i = min;
        break;

      case SignalType::FLOAT:
        if (smp->data[index].f > max)
          smp->data[index].f = max;

        if (smp->data[index].f < min)
          smp->data[index].f = min;
        break;

      case SignalType::INVALID:
      case SignalType::COMPLEX:
      case SignalType::BOOLEAN:
        return Hook::Reason::ERROR; // not supported
      }
    }

    return Reason::OK;
  }
};

// Register hook
static char n[] = "limit_value";
static char d[] = "Limit signal values";
static HookPlugin<LimitValueHook, n, d,
                  (int)Hook::Flags::PATH | (int)Hook::Flags::NODE_READ |
                      (int)Hook::Flags::NODE_WRITE>
    p;

} // namespace node
} // namespace villas
