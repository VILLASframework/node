/* PMU hook.
 *
 * Author: Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/dsp/window_cosine.hpp>
#include <villas/hook.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class PmuHook : public MultiSignalHook {

protected:
  enum class Status { INVALID, VALID };

  struct Phasor {
    double frequency;
    double amplitude;
    double phase;
    double rocof; // Rate of change of frequency.
    Status valid;
  };

  enum class WindowType { NONE, FLATTOP, HANN, HAMMING, NUTTAL, BLACKMAN };

  enum class TimeAlign {
    LEFT,
    CENTER,
    RIGHT,
  };

  std::vector<dsp::CosineWindow<double> *> windows;
  dsp::Window<timespec> *windowsTs;
  std::vector<Phasor> lastPhasors;

  enum TimeAlign timeAlignType;
  enum WindowType windowType;

  unsigned sampleRate;
  double phasorRate;
  double nominalFreq;
  double numberPlc;
  unsigned windowSize;
  bool channelNameEnable;
  double angleUnitFactor;
  uint64_t lastSequence;
  timespec nextRun;
  bool init;
  unsigned initSampleCount;

  // Correction factors.
  double phaseOffset;
  double amplitudeOffset;
  double frequencyOffset;
  double rocofOffset;

  virtual Phasor estimatePhasor(dsp::CosineWindow<double> *window,
                                const Phasor &lastPhasor);

public:
  PmuHook(Path *p, Node *n, int fl, int prio, bool en = true);

  void prepare() override;

  void parse(json_t *json) override;

  Hook::Reason process(struct Sample *smp) override;
};

} // namespace node
} // namespace villas
