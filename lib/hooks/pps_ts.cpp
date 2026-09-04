/* Timestamp hook.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <vector>

#include <villas/hook.hpp>
#include <villas/sample.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

class PpsTsHook : public SingleSignalHook {

protected:
  enum class TimeSource { OS, SAMPLE } timeSource;

  uint64_t lastSequence;
  bool firstSample;
  double lastValue;
  double threshold;
  bool armSecDetect;

  struct timespec tsVirt;
  double period; // In seconds
  uintmax_t cntEdges;
  uintmax_t cntSmps;

  unsigned currentSecond;
  std::vector<uintmax_t> filterWindow;

public:
  PpsTsHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : SingleSignalHook(p, n, fl, prio, en), timeSource(TimeSource::OS),
        lastSequence(0), firstSample(false), lastValue(0), threshold(1.5),
        armSecDetect(false), tsVirt({0, 0}), period(0.0), cntEdges(0),
        cntSmps(0), currentSecond(0) {}

  void parse(json_t *json) override {
    int ret;
    json_error_t err;

    assert(state != State::STARTED);

    SingleSignalHook::parse(json);

    const char *timeSourceC = nullptr;

    ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: f }", "time_source",
                         &timeSourceC, "threshold", &threshold);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-pps_ts");

    if (timeSourceC) {
      if (!strcmp(timeSourceC, "sample"))
        timeSource = TimeSource::SAMPLE;
      else
        timeSource = TimeSource::OS;
    }

    state = State::PARSED;
  }

  villas::node::Hook::Reason process(struct Sample *smp) override {
    assert(state == State::STARTED);

    // Get value of PPS signal
    float value = smp->data[signalIndex].f; // TODO check if it is really float

    if (!firstSample) {
      firstSample = true;
      lastValue = value;
      return Hook::Reason::SKIP_SAMPLE;
    }

    // Detect Edge
    bool isEdge = lastValue < threshold && value > threshold;

    if (isEdge) {

      if (cntEdges > 0) {
        tsVirt.tv_sec = currentSecond + 1;
        tsVirt.tv_nsec = 0;
        period = 1.0 / cntSmps;
        currentSecond = 0;
      }
      cntSmps = 0;
      cntEdges++;
      armSecDetect = true;
    } else {
      struct timespec tsPeriod = time_from_double(period);
      tsVirt = time_add(&tsVirt, &tsPeriod);
    }

    if (armSecDetect) {
      long current_nsec = 0;
      if (timeSource == TimeSource::OS)
        current_nsec = time_now().tv_nsec;
      else if (timeSource == TimeSource::SAMPLE)
        current_nsec = smp->ts.origin.tv_nsec;

      if (current_nsec > 0.5e9) {
        //take the second somewere in the center of the last second to reduce impact of system clock error
        if (timeSource == TimeSource::OS)
          currentSecond = time_now().tv_sec;
        else if (timeSource == TimeSource::SAMPLE)
          currentSecond = smp->ts.origin.tv_sec;
        armSecDetect = false;
      }
    }

    lastValue = value;
    cntSmps++;

    if (cntEdges < 2)
      return Hook::Reason::SKIP_SAMPLE;
    smp->ts.origin = tsVirt;
    smp->flags |= (int)SampleFlags::HAS_TS_ORIGIN;

    if ((smp->sequence - lastSequence) > 1)
      logger->warn("Samples missed: {} sampled missed",
                   smp->sequence - lastSequence);

    lastSequence = smp->sequence;
    return Hook::Reason::OK;
  }
};

// Register hook
static char n[] = "pps_ts";
static char d[] = "Timestamp samples based GPS PPS signal";
static HookPlugin<PpsTsHook, n, d,
                  (int)Hook::Flags::NODE_READ | (int)Hook::Flags::NODE_WRITE |
                      (int)Hook::Flags::PATH>
    p;

} // namespace node
} // namespace villas
