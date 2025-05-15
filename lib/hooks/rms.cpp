/* RMS hook.
 *
 * Author: Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/hook.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class RMSHook : public MultiSignalHook {

protected:
  std::vector<std::vector<double>> smpMemory;

  std::vector<double> accumulator;
  unsigned windowSize;
  uint64_t smpMemoryPosition;

public:
  RMSHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : MultiSignalHook(p, n, fl, prio, en), smpMemory(), windowSize(0),
        smpMemoryPosition(0) {}

  void prepare() override {
    MultiSignalHook::prepare();

    // Add signals
    for (auto index : signalIndices) {
      auto origSig = signals->getByIndex(index);

      // Check that signal has float type
      if (origSig->type != SignalType::FLOAT)
        throw RuntimeError(
            "The rms hook can only operate on signals of type float!");
    }

    /* Initialize memory for each channel*/
    smpMemory.clear();
    for (unsigned i = 0; i < signalIndices.size(); i++) {
      accumulator.push_back(0.0);
      smpMemory.emplace_back(windowSize, 0.0);
    }

    state = State::PREPARED;
  }

  void parse(json_t *json) override {
    int ret;
    int windowSizeIn;
    json_error_t err;

    assert(state != State::STARTED);

    MultiSignalHook::parse(json);

    ret =
        json_unpack_ex(json, &err, 0, "{ s: i }", "window_size", &windowSizeIn);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-rms");

    if (windowSizeIn < 1)
      throw ConfigError(json, "node-config-hook-rms",
                        "Window size must be greater 0 but is set to {}",
                        windowSizeIn);

    windowSize = (unsigned)windowSizeIn;

    state = State::PARSED;
  }

  Hook::Reason process(struct Sample *smp) override {
    assert(state == State::STARTED);

    unsigned i = 0;
    for (auto index : signalIndices) {
      // Square the new value
      double newValue = pow(smp->data[index].f, 2);

      // Get the old value from the history
      double oldValue = smpMemory[i][smpMemoryPosition % windowSize];

      // Append the new value to the history memory
      smpMemory[i][smpMemoryPosition % windowSize] = newValue;

      // Update the accumulator
      accumulator[index] += newValue;
      accumulator[index] -= oldValue;

      auto rms = pow(accumulator[index] / windowSize, 0.5);

      smp->data[index].f = rms;
      i++;
    }

    smpMemoryPosition++;

    return Reason::OK;
  }
};

// Register hook
static char n[] = "rms";
static char d[] = "This hook calculates the root-mean-square (RMS) on a window";
static HookPlugin<RMSHook, n, d,
                  (int)Hook::Flags::NODE_READ | (int)Hook::Flags::NODE_WRITE |
                      (int)Hook::Flags::PATH>
    p;

} // namespace node
} // namespace villas
