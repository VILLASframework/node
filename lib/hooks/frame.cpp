/* Frame hook.
 *
 * Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>

#include <jansson.h>

#include <villas/exceptions.hpp>
#include <villas/hook.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

enum class Trigger {
  TIMESTAMP,
  SEQUENCE,
};

enum class Unit {
  MILLISECONDS,
  SECONDS,
  MINUTES,
  HOURS,
};

uint64_t unit_wrap(std::optional<Unit> unit) {
  if (unit) {
    switch (*unit) {
    case Unit::HOURS:
      return 24;
    case Unit::MINUTES:
      return 60;
    case Unit::SECONDS:
      return 60;
    case Unit::MILLISECONDS:
      return 1000;
    }
  }

  return std::numeric_limits<uint64_t>::max();
}

class FrameHook : public Hook {
  using sample_ptr = std::unique_ptr<Sample, decltype(&sample_decref)>;

private:
  Trigger trigger;
  uint64_t interval;
  uint64_t offset;
  std::optional<Unit> unit;
  sample_ptr last_smp;

  bool updateInterval(Sample const *next_smp) {
    bool changed = false;

    if (!last_smp.get() ||
        (next_smp->flags & (int)SampleFlags::NEW_SIMULATION)) {
      changed = true;
    } else if (trigger == Trigger::SEQUENCE) {
      if (!(next_smp->flags & (int)SampleFlags::HAS_SEQUENCE))
        throw RuntimeError{"Missing sequence number."};

      auto last_interval = (last_smp->sequence + interval - offset) / interval;
      auto next_interval = (next_smp->sequence + interval - offset) / interval;
      changed = last_interval != next_interval;
    } else {
      if (!(next_smp->flags & (int)SampleFlags::HAS_TS_ORIGIN))
        throw RuntimeError{"Missing origin timestamp."};

      switch (unit.value()) {
      case Unit::HOURS: {
        auto last_hour = last_smp->ts.origin.tv_sec / 3'600;
        auto next_hour = next_smp->ts.origin.tv_sec / 3'600;

        auto last_day = last_hour / 24;
        auto next_day = next_hour / 24;
        if (last_day != next_day) {
          changed = true;
          break;
        }

        auto last_hour_of_day = last_hour - 24 * last_day;
        auto next_hour_of_day = next_hour - 24 * next_day;
        auto last_interval_of_day =
            (last_hour_of_day + interval - offset) / interval;
        auto next_interval_of_day =
            (next_hour_of_day + interval - offset) / interval;
        changed = last_interval_of_day != next_interval_of_day;
        break;
      }

      case Unit::MINUTES: {
        auto last_minute = last_smp->ts.origin.tv_sec / 60;
        auto next_minute = next_smp->ts.origin.tv_sec / 60;

        auto last_hour = last_minute / 60;
        auto next_hour = next_minute / 60;
        if (last_hour != next_hour) {
          changed = true;
          break;
        }

        auto last_minute_of_hour = last_minute - 60 * last_hour;
        auto next_minute_of_hour = next_minute - 60 * next_hour;
        auto last_interval_of_hour =
            (last_minute_of_hour + interval - offset) / interval;
        auto next_interval_of_hour =
            (next_minute_of_hour + interval - offset) / interval;
        changed = last_interval_of_hour != next_interval_of_hour;
        break;
      }

      case Unit::SECONDS: {
        auto last_second = last_smp->ts.origin.tv_sec;
        auto next_second = next_smp->ts.origin.tv_sec;

        auto last_minute = last_second / 60;
        auto next_minute = next_second / 60;
        if (last_minute != next_minute) {
          changed = true;
          break;
        }

        auto last_second_of_minute = last_second - 60 * last_minute;
        auto next_second_of_minute = next_second - 60 * next_minute;
        auto last_interval_of_minute =
            (last_second_of_minute + interval - offset) / interval;
        auto next_interval_of_minute =
            (next_second_of_minute + interval - offset) / interval;
        changed = last_interval_of_minute != next_interval_of_minute;
        break;
      }

      case Unit::MILLISECONDS: {
        auto last_second = last_smp->ts.origin.tv_sec;
        auto next_second = next_smp->ts.origin.tv_sec;
        if (last_second != next_second) {
          changed = true;
          break;
        }

        auto last_millisecond_of_second =
            last_smp->ts.origin.tv_nsec / 1'000'000;
        auto next_millisecond_of_second =
            next_smp->ts.origin.tv_nsec / 1'000'000;
        auto last_interval_of_second =
            (last_millisecond_of_second + interval - offset) / interval;
        auto next_interval_of_second =
            (next_millisecond_of_second + interval - offset) / interval;
        changed = last_interval_of_second != next_interval_of_second;
        break;
      }
      }
    }

    if (changed)
      logger->debug("new frame");

    return changed;
  }

public:
  FrameHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : Hook(p, n, fl, prio, en), trigger(Trigger::SEQUENCE), interval(1),
        offset(0), unit{std::nullopt}, last_smp{nullptr, &sample_decref} {}

  virtual ~FrameHook() { (void)last_smp.release(); }

  void parse(json_t *json) override {
    Hook::parse(json);

    char *trigger_str = nullptr;
    char *unit_str = nullptr;
    int interval_int = -1;
    int offset_int = -1;

    json_error_t err;
    auto ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: i, s?: i }",
                              "trigger", &trigger_str, "unit", &unit_str,
                              "interval", &interval_int, "offset", &offset_int);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-frame");

    if (trigger_str) {
      if (!strcmp(trigger_str, "sequence"))
        trigger = Trigger::SEQUENCE;
      else if (!strcmp(trigger_str, "timestamp"))
        trigger = Trigger::TIMESTAMP;
      else
        throw ConfigError(json, "node-config-hook-frame-unit");
    }

    if (trigger == Trigger::TIMESTAMP) {
      if (!strcmp(unit_str, "milliseconds"))
        unit = Unit::MILLISECONDS;
      else if (!strcmp(unit_str, "seconds"))
        unit = Unit::SECONDS;
      else if (!strcmp(unit_str, "minutes"))
        unit = Unit::MINUTES;
      else if (!strcmp(unit_str, "hours"))
        unit = Unit::HOURS;
      else
        throw ConfigError(json, "node-config-hook-frame-unit");
    }

    if (interval_int != -1) {
      if (interval_int <= 0 || (uint64_t)interval_int > unit_wrap(unit))
        throw ConfigError(json, "node-config-hook-frame-interval");

      interval = interval_int;
    }

    if (offset_int != -1) {
      if (offset_int < 0 || (uint64_t)offset_int >= unit_wrap(unit))
        throw ConfigError(json, "node-config-hook-frame-offset");

      offset = offset_int;
    }
  }

  virtual Hook::Reason process(Sample *smp) override {
    Hook::process(smp);

    if (updateInterval(smp))
      smp->flags |= (int)SampleFlags::NEW_FRAME;
    else
      smp->flags &= ~(int)SampleFlags::NEW_FRAME;

    sample_incref(smp);
    last_smp = sample_ptr{smp, &sample_decref};

    return Reason::OK;
  }

  virtual void stop() override {
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
