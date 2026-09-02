/* PMU hook.
 *
 * Author: Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/hooks/pmu.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

PmuHook::PmuHook(Path *p, Node *n, int fl, int prio, bool en)
    : MultiSignalHook(p, n, fl, prio, en), windows(), windowsTs(),
      timeAlignType(TimeAlign::CENTER), windowType(WindowType::NONE),
      sampleRate(1), dataRate(1), nominalFreq(1.0), numberPlc(1.),
      windowSize(1), channelNameEnable(true), angleUnitFactor(1.0),
      lastSequence(0), nextRun({0}), phasorRate(0), phasorPeriodNs(0),
      run(false), init(false), initSampleCount(0), phaseOffset(0.0),
      amplitudeOffset(0.0), frequencyOffset(0.0), rocofOffset(0.0) {}

void PmuHook::prepare() {
  MultiSignalHook::prepare();

  signals->clear();
  for (unsigned i = 0; i < signalIndices.size(); i++) {
    // Add signals
    auto freqSig =
        std::make_shared<Signal>("frequency", "Hz", SignalType::FLOAT);
    auto amplSig =
        std::make_shared<Signal>("amplitude", "V", SignalType::FLOAT);
    auto phasorSig =
        std::make_shared<Signal>("phasor", "V", SignalType::COMPLEX);
    auto phaseSig = std::make_shared<Signal>(
        "phase", (angleUnitFactor) ? "rad" : "deg",
        SignalType::FLOAT); //angleUnitFactor==1 means rad
    auto rocofSig =
        std::make_shared<Signal>("rocof", "Hz/s", SignalType::FLOAT);

    if (channelNameEnable) {
      auto suffix = fmt::format("_{}", signalNames[i]);

      freqSig->name += suffix;
      amplSig->name += suffix;
      phaseSig->name += suffix;
      rocofSig->name += suffix;
      phasorSig->name += suffix;
    }

    signals->push_back(freqSig);
    if (outputMode == OutputMode::COMPLEX) {
      signals->push_back(phasorSig);
    } else {
      signals->push_back(amplSig);
      signals->push_back(phaseSig);
    }

    signals->push_back(rocofSig);

    lastPhasors.push_back({0., 0., 0., 0.});
  }

  windowSize = ceil(sampleRate * numberPlc / nominalFreq);
  //phasorPeriodNs = static_cast<int64_t>(round(1.0e9 / phasorRate));

  for (unsigned i = 0; i < signalIndices.size(); i++) {
    if (windowType == WindowType::NONE)
      windows.push_back(new dsp::RectangularWindow<double>(windowSize, 0.0));
    else if (windowType == WindowType::FLATTOP)
      windows.push_back(new dsp::FlattopWindow<double>(windowSize, 0.0));
    else if (windowType == WindowType::HAMMING)
      windows.push_back(new dsp::HammingWindow<double>(windowSize, 0.0));
    else if (windowType == WindowType::HANN)
      windows.push_back(new dsp::HannWindow<double>(windowSize, 0.0));
    else if (windowType == WindowType::NUTTAL)
      windows.push_back(new dsp::NuttallWindow<double>(windowSize, 0.0));
    else if (windowType == WindowType::BLACKMAN)
      windows.push_back(new dsp::BlackmanWindow<double>(windowSize, 0.0));
  }

  windowsTs = new dsp::Window<timespec>(windowSize, timespec{0});
}

void PmuHook::parse(json_t *json) {
  MultiSignalHook::parse(json);
  int ret;

  const char *windowTypeC = nullptr;
  const char *angleUnitC = nullptr;
  const char *timeAlignC = nullptr;
  const char *outputModeC = nullptr;

  json_error_t err;

  assert(state != State::STARTED);

  ret = json_unpack_ex(
      json, &err, 0,
      "{ s?: i, s?: i, s?: F, s?: F, s?: s, s?: s, s?: b, s?: s, s?: F, s?: F, "
      "s?: F, s?: F, s?: s}",
      "sample_rate", &sampleRate, "dft_rate", &dataRate, "nominal_freq",
      &nominalFreq, "number_plc", &numberPlc, "window_type", &windowTypeC,
      "angle_unit", &angleUnitC, "add_channel_name", &channelNameEnable,
      "timestamp_align", &timeAlignC, "phase_offset", &phaseOffset,
      "amplitude_offset", &amplitudeOffset, "frequency_offset",
      &frequencyOffset, "rocof_offset", &rocofOffset, "output_mode",
      &outputModeC);

  if (ret)
    throw ConfigError(json, err, "node-config-hook-pmu");

  if (sampleRate <= 0)
    throw ConfigError(json, "node-config-hook-pmu-sample_rate",
                      "Sample rate cannot be less than 0 tried to set {}",
                      sampleRate);

  if (dataRate == 0)
    throw ConfigError(json, "node-config-hook-pmu-data_rate",
                      "Data rate cannot be set to 0");

  if (nominalFreq <= 0)
    throw ConfigError(json, "node-config-hook-pmu-nominal_freq",
                      "Nominal frequency cannot be less than 0 tried to set {}",
                      nominalFreq);

  if (numberPlc <= 0)
    throw ConfigError(
        json, "node-config-hook-pmu-number_plc",
        "Number of power line cycles cannot be less than 0 tried to set {}",
        numberPlc);

  if (!outputModeC)
    outputMode = OutputMode::FLOAT;
  else if (strcmp(outputModeC, "complex") == 0)
    outputMode = OutputMode::COMPLEX;
  else
    outputMode = OutputMode::FLOAT;

  if (!windowTypeC)
    logger->info("No Window type given, assume no windowing");
  else if (strcmp(windowTypeC, "flattop") == 0)
    windowType = WindowType::FLATTOP;
  else if (strcmp(windowTypeC, "hamming") == 0)
    windowType = WindowType::HAMMING;
  else if (strcmp(windowTypeC, "hann") == 0)
    windowType = WindowType::HANN;
  else if (strcmp(windowTypeC, "nuttal") == 0)
    windowType = WindowType::NUTTAL;
  else if (strcmp(windowTypeC, "blackman") == 0)
    windowType = WindowType::BLACKMAN;
  else
    throw ConfigError(json, "node-config-hook-pmu-window-type",
                      "Invalid window type: {}", windowTypeC);

  if (!angleUnitC)
    logger->info("No angle type given, assume rad");
  else if (strcmp(angleUnitC, "rad") == 0)
    angleUnitFactor = 1;
  else if (strcmp(angleUnitC, "degree") == 0)
    angleUnitFactor = 180 / M_PI;
  else
    throw ConfigError(json, "node-config-hook-pmu-angle-unit",
                      "Angle unit {} not recognized", angleUnitC);

  if (!timeAlignC)
    logger->info("No timestamp alignment defined. Assume alignment center");
  else if (strcmp(timeAlignC, "left") == 0)
    timeAlignType = TimeAlign::LEFT;
  else if (strcmp(timeAlignC, "center") == 0)
    timeAlignType = TimeAlign::CENTER;
  else if (strcmp(timeAlignC, "right") == 0)
    timeAlignType = TimeAlign::RIGHT;
  else
    throw ConfigError(json, "node-config-hook-dft-timestamp-alignment",
                      "Timestamp alignment {} not recognized", timeAlignC);
}

timespec PmuHook::calcNextRun(timespec currentTimetag) {

  timespec nextPhasor = currentTimetag;

  timespec offset_ts = {.tv_sec = 0, .tv_nsec = 1};
  if (timeAlignType == TimeAlign::CENTER) {
    auto offset_ns = 1'000'000'000ll * windowSize / (sampleRate * 2);
    offset_ts = {
        .tv_sec = offset_ns / 1'000'000'000ll,
        .tv_nsec = offset_ns % 1'000'000'000ll,
    };
  } else if (timeAlignType == TimeAlign::LEFT) {
    auto offset_ns = 1'000'000'000ll * windowSize / (sampleRate);
    offset_ts = {
        .tv_sec = offset_ns / 1'000'000'000ll,
        .tv_nsec = offset_ns % 1'000'000'000ll,
    };
  }
  currentTimetag = time_sub(&currentTimetag, &offset_ts);
  if (dataRate > 0) {
    //handle rates of multiple phasors per second. dataRate is the phasors per second

    int n = (currentTimetag.tv_nsec * dataRate) / 1'000'000'000l;
    int next = (n + 1) % dataRate;
    nextPhasor = {
        .tv_sec =
            (next <= n) ? currentTimetag.tv_sec + 1 : currentTimetag.tv_sec,
        .tv_nsec = (next * 1'000'000'000l) / dataRate,
    };
  } else {
    //handle rates of less then one phasor per second. dataRate is the seconds between phasors
    int period = -dataRate;
    int hour = currentTimetag.tv_sec / 3600;
    int n = (currentTimetag.tv_sec - hour * 3600) / period;
    int next = n + 1;
    if (next * period > 3600) {
      hour += 1;
      next = 0;
    }

    nextPhasor = {
        .tv_sec = hour * 3600 + period * next,
        .tv_nsec = 0,
    };
  }

  return time_add(&nextPhasor, &offset_ts);
}

Hook::Reason PmuHook::process(struct Sample *smp) {
  assert(state == State::STARTED);

  initSampleCount++;

  if (smp->sequence - lastSequence > 1)
    logger->warn("Calculation is not Realtime. {} sampled missed",
                 smp->sequence - lastSequence);
  lastSequence = smp->sequence;

  // Update sample memory before run
  unsigned i = 0;
  for (auto index : signalIndices)
    windows[i++]->update(smp->data[index].f);
  windowsTs->update(smp->ts.origin);

  if (!init && initSampleCount >= windowSize) {
    init = true;
    nextRun = calcNextRun(smp->ts.origin);
  }

  timespec timeDiff = time_diff(&nextRun, &smp->ts.origin);
  int64_t timeDiffNs =
      static_cast<int64_t>(timeDiff.tv_sec) * 1'000'000'000l + timeDiff.tv_nsec;

  if (!run && timeDiffNs >= (-1'000'000'000l /
                             sampleRate)) { //timeDiffNs >= -1e9/sampleRate
    run = true;
  } else if (run && timeDiffNs > 0) {
    nextRun = calcNextRun(smp->ts.origin);
    run = false;
  } else if (run)
    return Reason::SKIP_SAMPLE;

  Status phasorStatus = Status::VALID;
  timespec phasorTimestamp = {0};
  if (run) {
    for (unsigned i = 0; i < signalIndices.size(); i++) {
      lastPhasors[i] = estimatePhasor(windows[i], lastPhasors[i]);
      if (lastPhasors[i].valid != Status::VALID)
        phasorStatus = Status::INVALID;
    }

    size_t tsPos = 0;
    if (timeAlignType == TimeAlign::RIGHT)
      tsPos = windowSize - 1;
    else if (timeAlignType == TimeAlign::LEFT)
      tsPos = 0;
    else if (timeAlignType == TimeAlign::CENTER)
      tsPos = windowSize / 2;
    phasorTimestamp = (*windowsTs)[tsPos];
  }

  // Make sure to update phasors after window update but estimate them before
  if (run) {
    for (unsigned i = 0; i < signalIndices.size(); i++) {
      if (outputMode == OutputMode::COMPLEX) {
        smp->data[i * 3 + 0].f =
            lastPhasors[i].frequency + frequencyOffset; // Frequency
        smp->data[i * 3 + 1].z =
            std::polar(lastPhasors[i].amplitude / std::numbers::sqrt2,
                       lastPhasors[i].phase);                        // Phasor
        smp->data[i * 3 + 2].f = lastPhasors[i].rocof + rocofOffset; // ROCOF
        smp->length = signalIndices.size() * 3;
        smp->ts.origin = phasorTimestamp;
      } else {
        smp->data[i * 4 + 0].f =
            lastPhasors[i].frequency + frequencyOffset; // Frequency
        smp->data[i * 4 + 1].f = (lastPhasors[i].amplitude / pow(2, 0.5)) +
                                 amplitudeOffset; // Amplitude
        smp->data[i * 4 + 2].f =
            (lastPhasors[i].phase * 180 / M_PI) + phaseOffset;       // Phase
        smp->data[i * 4 + 3].f = lastPhasors[i].rocof + rocofOffset; /* ROCOF */
        smp->length = signalIndices.size() * 4;
        smp->ts.origin = phasorTimestamp;
      }
    }
  }

  if (!run || phasorStatus != Status::VALID)
    return Reason::SKIP_SAMPLE;

  return Reason::OK;
}

PmuHook::Phasor PmuHook::estimatePhasor(dsp::CosineWindow<double> *window,
                                        const Phasor &lastPhasor) {
  return {0., 0., 0., 0., Status::INVALID};
}

// Register hook
static char n[] = "pmu";
static char d[] = "This hook estimates a phasor";
static HookPlugin<PmuHook, n, d,
                  (int)Hook::Flags::NODE_READ | (int)Hook::Flags::NODE_WRITE |
                      (int)Hook::Flags::PATH>
    p;

} // namespace node
} // namespace villas
