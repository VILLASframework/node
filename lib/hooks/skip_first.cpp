/* Skip first hook.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/hook.hpp>
#include <villas/sample.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

class SkipFirstHook : public Hook {

protected:
  enum class SkipState {
    STARTED,  // Path just started. First sample not received yet.
    SKIPPING, // First sample received. Skipping samples now.
    NORMAL    // All samples skipped. Normal operation.
  } skip_state;

  enum class Mode { SECONDS, SAMPLES } mode;

  union {
    struct {
      timespec until;
      timespec wait; // Absolute point in time from where we accept samples.
    } seconds;

    struct {
      uint64_t until;
      int wait;
    } samples;
  };

public:
  using Hook::Hook;

  void parse(json_t *json) override {
    double s;

    int ret;
    json_error_t err;

    assert(state != State::STARTED);

    Hook::parse(json);

    ret = json_unpack_ex(json, &err, 0, "{ s: F }", "seconds", &s);
    if (!ret) {
      seconds.wait = time_from_double(s);
      mode = Mode::SECONDS;

      state = State::PARSED;
      return;
    }

    ret = json_unpack_ex(json, &err, 0, "{ s: i }", "samples", &samples.wait);
    if (!ret) {
      mode = Mode::SAMPLES;

      state = State::PARSED;
      return;
    }

    throw ConfigError(json, err, "node-config-hook-skip_first");
  }

  void start() override {
    skip_state = SkipState::STARTED;
    state = State::STARTED;
  }

  void restart() override { skip_state = SkipState::STARTED; }

  Hook::Reason process(struct Sample *smp) override {
    assert(state == State::STARTED);

    // Remember sequence no or timestamp of first sample.
    if (skip_state == SkipState::STARTED) {
      switch (mode) {
      case Mode::SAMPLES:
        samples.until = smp->sequence + samples.wait;
        break;

      case Mode::SECONDS:
        seconds.until = time_add(&smp->ts.origin, &seconds.wait);
        break;
      }

      skip_state = SkipState::SKIPPING;
    }

    switch (mode) {
    case Mode::SAMPLES:
      if (samples.until > smp->sequence)
        return Hook::Reason::SKIP_SAMPLE;
      break;

    case Mode::SECONDS:
      if (time_delta(&seconds.until, &smp->ts.origin) < 0)
        return Hook::Reason::SKIP_SAMPLE;
      break;

    default:
      break;
    }

    return Reason::OK;
  }
};

// Register hook
static char n[] = "skip_first";
static char d[] = "Skip the first samples";
static HookPlugin<SkipFirstHook, n, d,
                  (int)Hook::Flags::NODE_READ | (int)Hook::Flags::NODE_WRITE |
                      (int)Hook::Flags::PATH>
    p;

} // namespace node
} // namespace villas
