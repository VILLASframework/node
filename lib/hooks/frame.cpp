/* Frame hook.
 *
 * Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <cstdint>
#include <cstring>
#include <variant>

#include <jansson.h>

#include <villas/config_helper.hpp>
#include <villas/exceptions.hpp>
#include <villas/hook.hpp>
#include <villas/sample.hpp>

#include "villas/timing.hpp"

namespace villas {
namespace node {

class FrameHook : public Hook {
  using TimeInterval = std::chrono::milliseconds;
  using SequenceInterval = uint64_t;

private:
  std::variant<TimeInterval, SequenceInterval> interval;
  Sample::PtrUnique last_smp;

  bool updateInterval(Sample const *next_smp) {
    bool changed = false;

    if (!last_smp.get() ||
        (next_smp->flags & (int)SampleFlags::NEW_SIMULATION)) {
      changed = true;
    } else if (auto *i = std::get_if<SequenceInterval>(&interval)) {
      if (!(next_smp->flags & (int)SampleFlags::HAS_SEQUENCE))
        throw RuntimeError("Missing sequence number");

      auto last_interval = (last_smp->sequence + *i) / *i;
      auto next_interval = (next_smp->sequence + *i) / *i;

      changed = last_interval != next_interval;
    } else if (auto *i = std::get_if<TimeInterval>(&interval)) {
      if (!(next_smp->flags & (int)SampleFlags::HAS_TS_ORIGIN))
        throw RuntimeError("Missing origin timestamp");

      auto last_ts = time_to_timepoint<TimeInterval>(&last_smp->ts.origin);
      auto next_ts = time_to_timepoint<TimeInterval>(&next_smp->ts.origin);

      auto last_interval = (last_ts.time_since_epoch() + *i) / *i;
      auto next_interval = (next_ts.time_since_epoch() + *i) / *i;

      changed = last_interval != next_interval;
    }

    return changed;
  }

public:
  FrameHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : Hook(p, n, fl, prio, en), interval(TimeInterval(0)),
        last_smp{nullptr, &sample_decref} {}

  ~FrameHook() override { (void)last_smp.release(); }

  void parse(json_t *json) override {
    Hook::parse(json);

    char *trigger_str = nullptr;
    json_t *json_interval = nullptr;

    json_error_t err;
    auto ret = json_unpack_ex(json, &err, 0, "{ s?: s, s: o }", "trigger",
                              &trigger_str, "interval", &json_interval);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-frame");

    if (trigger_str == nullptr || !strcmp(trigger_str, "timestamp")) {
      interval = parse_duration<TimeInterval>(json_interval);

      if (std::get<TimeInterval>(interval) == TimeInterval::zero())
        throw ConfigError(json, "node-config-hook-frame-interval",
                          "Interval must be greater than zero");
    } else if (!strcmp(trigger_str, "sequence")) {
      if (!json_is_integer(json_interval))
        throw ConfigError(json_interval, "node-config-hook-frame-interval",
                          "Interval must be an integer");

      interval = SequenceInterval(json_integer_value(json_interval));
    }
  }

  Hook::Reason process(Sample *smp) override {
    Hook::process(smp);

    if (updateInterval(smp))
      smp->flags |= (int)SampleFlags::NEW_FRAME;
    else
      smp->flags &= ~(int)SampleFlags::NEW_FRAME;

    sample_incref(smp);
    last_smp = Sample::PtrUnique{smp, &sample_decref};

    return Reason::OK;
  }

  void stop() override {
    Hook::stop();

    last_smp = nullptr;
  }
};

// Register hook
static char n[] = "frame";
static char d[] = "Add frame annotations too the stream of samples";
static HookPlugin<FrameHook, n, d, (int)Hook::Flags::PATH, 10> p;

} // namespace node
} // namespace villas
