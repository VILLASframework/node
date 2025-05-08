/* Gate hook.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cmath>
#include <limits>
#include <string>

#include <villas/hook.hpp>
#include <villas/node.hpp>
#include <villas/sample.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

class GateHook : public SingleSignalHook {

protected:
  enum class Mode { ABOVE, BELOW, RISING_EDGE, FALLING_EDGE } mode;

  double threshold;
  double duration;
  int samples;
  double previousValue;

  bool active;
  uint64_t startSequence;
  timespec startTime;

public:
  GateHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : SingleSignalHook(p, n, fl, prio, en), mode(Mode::RISING_EDGE),
        threshold(0.5), duration(-1), samples(-1),
        previousValue(std::numeric_limits<double>::quiet_NaN()), active(false),
        startSequence(0) {}

  void parse(json_t *json) override {
    int ret;

    json_error_t err;

    const char *mode_str;

    assert(state != State::STARTED);

    SingleSignalHook::parse(json);

    ret = json_unpack_ex(json, &err, 0, "{ s?: F, s?: F, s?: i, s?: s }",
                         "threshold", &threshold, "duration", &duration,
                         "samples", &samples, "mode", &mode_str);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-gate");

    if (mode_str) {
      if (!strcmp(mode_str, "above"))
        mode = Mode::ABOVE;
      else if (!strcmp(mode_str, "below"))
        mode = Mode::BELOW;
      else if (!strcmp(mode_str, "rising_edge"))
        mode = Mode::RISING_EDGE;
      else if (!strcmp(mode_str, "falling_edge"))
        mode = Mode::FALLING_EDGE;
    }

    state = State::PARSED;
  }

  void prepare() override {
    assert(state == State::CHECKED);

    SingleSignalHook::prepare();

    // Check if signal type is float
    auto sig = signals->getByIndex(signalIndex);
    if (sig->type != SignalType::FLOAT)
      throw RuntimeError("Gate signal must be of type float");

    state = State::PREPARED;
  }

  Hook::Reason process(struct Sample *smp) override {
    assert(state == State::STARTED);

    Hook::Reason reason;
    double value = smp->data[signalIndex].f;

    if (active) {
      if (duration > 0 && time_delta(&smp->ts.origin, &startTime) < duration)
        reason = Reason::OK;
      else if (samples > 0 && smp->sequence - startSequence < (uint64_t)samples)
        reason = Reason::OK;
      else {
        reason = Reason::SKIP_SAMPLE;
        active = false;
      }
    }

    if (!active) {
      switch (mode) {
      case Mode::ABOVE:
        reason = value > threshold ? Reason::OK : Reason::SKIP_SAMPLE;
        break;

      case Mode::BELOW:
        reason = value < threshold ? Reason::OK : Reason::SKIP_SAMPLE;
        break;

      case Mode::RISING_EDGE:
        reason = (!std::isnan(previousValue) && value > previousValue)
                     ? Reason::OK
                     : Reason::SKIP_SAMPLE;
        break;

      case Mode::FALLING_EDGE:
        reason = (!std::isnan(previousValue) && value < previousValue)
                     ? Reason::OK
                     : Reason::SKIP_SAMPLE;
        break;

      default:
        reason = Reason::ERROR;
      }

      if (reason == Reason::OK) {
        startTime = smp->ts.origin;
        startSequence = smp->sequence;
        active = true;
      }
    }

    previousValue = value;

    return reason;
  }
};

// Register hook
static char n[] = "gate";
static char d[] =
    "Skip samples only if an enable signal is under a specified threshold";
static HookPlugin<GateHook, n, d,
                  (int)Hook::Flags::NODE_READ | (int)Hook::Flags::PATH>
    p;

} // namespace node
} // namespace villas
