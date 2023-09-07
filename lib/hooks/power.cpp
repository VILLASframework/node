/* RMS hook.
 *
 * Author: Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/hook.hpp>
#include <villas/sample.hpp>

#include <iostream>

namespace villas {
namespace node {

/* Concept: Based on RMS Hook
 *
 * For each window, calculate integrals for U, I, U*I
 * Formulas from: https://de.wikipedia.org/wiki/Scheinleistung
 * Calculate S and P from these
 */
class PowerHook : public MultiSignalHook {

protected:
  enum class TimeAlign {
    LEFT,
    CENTER,
    RIGHT,
  };

  struct PowerPairing {
    int voltageIndex;
    int currentIndex;
  };

  struct PairingsStr {
    std::string voltage;
    std::string current;
  };

  std::vector<PairingsStr> pairingsStr;
  std::vector<std::vector<double>> smpMemory;
  std::vector<PowerPairing> pairings;
  std::vector<timespec> smpMemoryTs;

  std::vector<double> accumulator_u;  // Integral over u
  std::vector<double> accumulator_i;  // Integral over I
  std::vector<double> accumulator_ui; // Integral over U*I

  unsigned windowSize;
  uint64_t smpMemoryPosition;
  bool calcActivePower;
  bool calcReactivePower;
  bool caclApparentPower;
  bool calcCosPhi;
  bool
      channelNameEnable; // Rename the output values with channel name or only descriptive name
  double angleUnitFactor;
  enum TimeAlign timeAlignType;

public:
  PowerHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : MultiSignalHook(p, n, fl, prio, en), smpMemory(), pairings(),
        smpMemoryTs(), windowSize(0), smpMemoryPosition(0),
        calcActivePower(true), calcReactivePower(true), caclApparentPower(true),
        calcCosPhi(true), channelNameEnable(false), angleUnitFactor(1),
        timeAlignType(TimeAlign::CENTER) {}

  virtual void prepare() {
    MultiSignalHook::prepare();

    for (unsigned i = 0; i < pairingsStr.size(); i++) {
      PowerPairing p = {
          .voltageIndex = signals->getIndexByName(pairingsStr[i].voltage),
          .currentIndex = signals->getIndexByName(pairingsStr[i].current)};

      if (p.currentIndex == -1)
        throw RuntimeError("Pairings signal name not recognized {}",
                           pairingsStr[i].current);
      if (p.voltageIndex == -1)
        throw RuntimeError("Pairings signal name not recognized {}",
                           pairingsStr[i].voltage);

      pairings.push_back(p);
    }

    if ((pairingsStr.size() * 2) != signalIndices.size())
      throw RuntimeError("Number of signals and parings don not match!");

    // Check Signal Data Type
    for (auto index : signalIndices) {
      auto origSig = signals->getByIndex(index);

      // Check that signal has float type
      if (origSig->type != SignalType::FLOAT)
        throw RuntimeError(
            "The power hook can only operate on signals of type float!");
    }

    signals->clear();
    for (unsigned i = 0; i < signalIndices.size(); i++) {
      std::string suffix = "";
      if (channelNameEnable)
        suffix = fmt::format("_{}", signalNames[i]);

      // Add signals
      if (calcActivePower) {
        auto activeSig =
            std::make_shared<Signal>("active", "W", SignalType::FLOAT);
        activeSig->name += suffix;
        if (!activeSig)
          throw RuntimeError("Failed to create new signals");
        signals->push_back(activeSig);
      }

      if (calcReactivePower) {
        auto reactiveSig =
            std::make_shared<Signal>("reactive", "Var", SignalType::FLOAT);
        reactiveSig->name += suffix;
        if (!reactiveSig)
          throw RuntimeError("Failed to create new signals");
        signals->push_back(reactiveSig);
      }

      if (caclApparentPower) {
        auto apparentSig =
            std::make_shared<Signal>("apparent", "VA", SignalType::FLOAT);
        apparentSig->name += suffix;
        if (!apparentSig)
          throw RuntimeError("Failed to create new signals");
        signals->push_back(apparentSig);
      }

      if (calcCosPhi) {
        auto cosPhiSig =
            std::make_shared<Signal>("cos_phi", "deg", SignalType::FLOAT);
        cosPhiSig->name += suffix;
        if (!cosPhiSig)
          throw RuntimeError("Failed to create new signals");
        signals->push_back(cosPhiSig);
      }
    }

    // Initialize sampMemory to 0
    smpMemory.clear();
    for (unsigned i = 0; i < signalIndices.size(); i++)
      smpMemory.emplace_back(windowSize, 0.0);

    smpMemoryTs.clear();
    for (unsigned i = 0; i < windowSize; i++)
      smpMemoryTs.push_back({0});

    // Init empty accumulators for each pairing
    for (size_t i = 0; i < pairings.size(); i++) {
      accumulator_i.push_back(0.0);
      accumulator_u.push_back(0.0);
      accumulator_ui.push_back(0.0);
    }

    // Signal state prepared
    state = State::PREPARED;
  }

  // Read configuration JSON and configure hook accordingly
  virtual void parse(json_t *json) {
    // Ensure hook is not yet running
    assert(state != State::STARTED);

    int result = 0;

    const char *timeAlignC = nullptr;
    const char *angleUnitC = nullptr;

    json_error_t json_error;

    int windowSizeIn = 0; // Size of window in samples

    json_t *json_pairings = nullptr;

    MultiSignalHook::parse(json);

    result = json_unpack_ex(
        json, &json_error, 0,
        "{ s: i , s: o, s?: b, s?: b, s?: b, s?: b, s?: b, s? : s, s? : s }",
        "window_size", &windowSizeIn, "pairings", &json_pairings,
        "add_channel_name", &channelNameEnable, "active_power",
        &calcActivePower, "reactive_power", &calcReactivePower,
        "apparent_power", &caclApparentPower, "cos_phi", &calcCosPhi,
        "timestamp_align", &timeAlignC, "angle_unit", &angleUnitC);

    if (result)
      throw ConfigError(json, json_error, "node-config-hook-power");

    if (windowSizeIn < 1)
      throw ConfigError(json, "node-config-hook-power",
                        "Window size must be greater 0 but is set to {}",
                        windowSizeIn);

    windowSize = (unsigned)windowSizeIn;

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

    if (!angleUnitC)
      logger->info("No angle type given, assume rad");
    else if (strcmp(angleUnitC, "rad") == 0)
      angleUnitFactor = 1;
    else if (strcmp(angleUnitC, "degree") == 0)
      angleUnitFactor = 180 / M_PI;
    else
      throw ConfigError(json, "node-config-hook-dft-angle-unit",
                        "Angle unit {} not recognized", angleUnitC);

    // Pairings
    if (!json_is_array(json_pairings))
      throw ConfigError(json_pairings, "node-config-hook-power",
                        "Pairings are expected as json array");

    size_t i = 0;
    json_t *json_pairings_value;
    json_array_foreach(json_pairings, i, json_pairings_value) {
      const char *voltageNameC = nullptr;
      const char *currentNameC = nullptr;

      json_unpack_ex(json_pairings_value, &json_error, 0, "{ s: s, s: s}",
                     "voltage", &voltageNameC, "current", &currentNameC);
      pairingsStr.push_back(
          (PairingsStr){.voltage = voltageNameC, .current = currentNameC});
    }

    state = State::PARSED;
  }

  // This function does the actual processing of the hook when a new sample is passed through.
  virtual Hook::Reason process(struct Sample *smp) {
    assert(state == State::STARTED);

    uint sigIndex = 0; //used to set the pos of value in output sampel array

    smpMemoryTs[smpMemoryPosition % windowSize] = smp->ts.origin;

    // Loop over all pairings
    for (size_t i = 0; i < pairings.size(); i++) {
      auto pair = pairings[i];

      // Update U integral
      double oldValueU =
          smpMemory[pair.voltageIndex][smpMemoryPosition % windowSize];
      double newValueU = smp->data[pair.voltageIndex].f;
      smpMemory[pair.voltageIndex][smpMemoryPosition % windowSize] =
          newValueU; // Save for later

      accumulator_u[i] -= oldValueU * oldValueU;
      accumulator_u[i] += newValueU * newValueU;

      // Update I integral
      double newValueI = smp->data[pair.currentIndex].f;
      double oldValueI =
          smpMemory[pair.currentIndex][smpMemoryPosition % windowSize];
      smpMemory[pair.currentIndex][smpMemoryPosition % windowSize] =
          newValueI; // Save for later

      accumulator_i[i] -= oldValueI * oldValueI;
      accumulator_i[i] += newValueI * newValueI;

      // Update UI Integral
      accumulator_ui[i] -= oldValueI * oldValueU;
      accumulator_ui[i] += newValueI * newValueU;

      // Calc active power power
      double P = (1.0 / windowSize) * accumulator_ui[i];

      // Calc apparent power
      double S =
          (1.0 / windowSize) * pow(accumulator_i[i] * accumulator_u[i], 0.5);

      // Calc reactive power
      double Q = pow(S * S - P * P, 0.5);

      // Calc cos phi
      double PHI = atan2(Q, P) * angleUnitFactor;

      if (smpMemoryPosition >= windowSize) {
        // Write to samples
        if (calcActivePower)
          smp->data[sigIndex++].f = P;
        if (calcReactivePower)
          smp->data[sigIndex++].f = Q;
        if (caclApparentPower)
          smp->data[sigIndex++].f = S;
        if (calcCosPhi)
          smp->data[sigIndex++].f = PHI;
      }
    }

    smp->length = sigIndex;

    if (smpMemoryPosition >= windowSize) {
      unsigned tsPos = 0;
      if (timeAlignType == TimeAlign::RIGHT)
        tsPos = smpMemoryPosition;
      else if (timeAlignType == TimeAlign::LEFT)
        tsPos = smpMemoryPosition - windowSize;
      else if (timeAlignType == TimeAlign::CENTER)
        tsPos = smpMemoryPosition - (windowSize / 2);

      smp->ts.origin = smpMemoryTs[tsPos % windowSize];
    }

    if (smpMemoryPosition >=
        2 * windowSize) //reset smpMemPos if greater than twice the window. Important to handle init
      smpMemoryPosition = windowSize;

    smpMemoryPosition++; // Move write head for sample history foreward by one
    if (windowSize < smpMemoryPosition)
      return Reason::OK;

    return Reason::SKIP_SAMPLE;
  }
};

// Register hook
static char n[] = "power";
static char d[] =
    "This hook calculates the Active and Reactive Power for a given signal ";
static HookPlugin<PowerHook, n, d,
                  (int)Hook::Flags::NODE_READ | (int)Hook::Flags::NODE_WRITE |
                      (int)Hook::Flags::PATH>
    p;

} // namespace node
} // namespace villas
