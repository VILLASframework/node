/* Average hook.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/hook.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>

namespace villas {
namespace node {

class AverageHook : public MultiSignalHook {

protected:
  unsigned offset;

public:
  AverageHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : MultiSignalHook(p, n, fl, prio, en), offset(0) {}

  void prepare() override {
    assert(state == State::CHECKED);

    MultiSignalHook::prepare();

    // Add averaged signal
    auto avg_sig = std::make_shared<Signal>("average", "", SignalType::FLOAT);
    if (!avg_sig)
      throw RuntimeError("Failed to create new signal");

    signals->insert(signals->begin() + offset, avg_sig);

    state = State::PREPARED;
  }

  void parse(json_t *json) override {
    int ret;
    json_error_t err;

    assert(state != State::STARTED);

    MultiSignalHook::parse(json);

    ret = json_unpack_ex(json, &err, 0, "{ s: i }", "offset", &offset);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-average");

    state = State::PARSED;
  }

  Hook::Reason process(struct Sample *smp) override {
    double avg, sum = 0;
    int n = 0;

    assert(state == State::STARTED);

    for (unsigned index : signalIndices) {
      switch (sample_format(smp, index)) {
      case SignalType::INTEGER:
        sum += smp->data[index].i;
        break;

      case SignalType::FLOAT:
        sum += smp->data[index].f;
        break;

      case SignalType::INVALID:
      case SignalType::COMPLEX:
      case SignalType::BOOLEAN:
        return Hook::Reason::ERROR; // not supported
      }

      n++;
    }

    avg = sum / n;

    if (offset >= smp->length)
      return Reason::ERROR;

    sample_data_insert(smp, (union SignalData *)&avg, offset, 1);

    return Reason::OK;
  }
};

// Register hook
static char n[] = "average";
static char d[] = "Calculate average over some signals";
static HookPlugin<AverageHook, n, d,
                  (int)Hook::Flags::PATH | (int)Hook::Flags::NODE_READ |
                      (int)Hook::Flags::NODE_WRITE>
    p;

} // namespace node
} // namespace villas
