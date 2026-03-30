/* DFT hook.
 *
 * Author: Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <complex>
#include <cstring>
#include <vector>

#include <villas/dumper.hpp>
#include <villas/hook.hpp>
#include <villas/sample.hpp>
#include <villas/timing.hpp>

// Uncomment to enable dumper of memory windows
//#define DFT_MEM_DUMP

namespace villas {
namespace node {

class PmuDftHook : public MultiSignalHook {

protected:
  enum class PaddingType { ZERO, SIG_REPEAT };

  enum class WindowType { NONE, FLATTOP, HANN, HAMMING };

  enum class EstimationType { NONE, QUADRATIC, IpDFT };

  enum class TimeAlign {
    LEFT,
    CENTER,
    RIGHT,
  };

  struct Point {
    double x;
    std::complex<double> y;
  };

  struct DftEstimate {
    double amplitude;
    double frequency;
    double phase;
  };

  struct Phasor {
    double frequency;
    double amplitude;
    double phase;
    double rocof; // Rate of change of frequency.
  };

  enum WindowType windowType;
  enum PaddingType paddingType;
  enum EstimationType estType;
  enum TimeAlign timeAlignType;

  std::vector<std::vector<double>> smpMemoryData;
  std::vector<timespec> smpMemoryTs;
#ifdef DFT_MEM_DUMP
  std::vector<double> ppsMemory;
#endif
  std::vector<std::vector<std::complex<double>>> matrix;
  std::vector<std::vector<std::complex<double>>> results;
  std::vector<double> filterWindowCoefficents;
  std::vector<std::vector<double>> absResults;
  std::vector<double> absFrequencies;

  uint64_t calcCount;
  unsigned sampleRate;
  double startFrequency;
  double endFreqency;
  double frequencyResolution;
  unsigned rate;
  unsigned ppsIndex;
  unsigned windowSize;
  unsigned
      windowMultiplier; // Multiplyer for the window to achieve frequency resolution
  unsigned freqCount; // Number of requency bins that are calculated
  bool
      channelNameEnable; // Rename the output values with channel name or only descriptive name

  uint64_t smpMemPos;
  uint64_t lastSequence;

  std::complex<double> omega;

  double windowCorrectionFactor;
  struct timespec lastCalc;
  double nextCalc;

  std::vector<Phasor> lastResult;

  std::string dumperPrefix;
  bool dumperEnable;
#ifdef DFT_MEM_DUMP
  Dumper origSigSync;
  Dumper windowdSigSync;
  Dumper ppsSigSync;
  Dumper phasorRocof;
  Dumper phasorPhase;
  Dumper phasorAmplitude;
  Dumper phasorFreq;
#endif

  double angleUnitFactor;
  double phaseOffset;
  double frequencyOffset;
  double amplitudeOffset;
  double rocofOffset;

public:
  PmuDftHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : MultiSignalHook(p, n, fl, prio, en), windowType(WindowType::NONE),
        paddingType(PaddingType::ZERO), estType(EstimationType::NONE),
        timeAlignType(TimeAlign::CENTER), smpMemoryData(), smpMemoryTs(),
#ifdef DFT_MEM_DUMP
        ppsMemory(),
#endif
        matrix(), results(), filterWindowCoefficents(), absResults(),
        absFrequencies(), calcCount(0), sampleRate(0), startFrequency(0),
        endFreqency(0), frequencyResolution(0), rate(0), ppsIndex(0),
        windowSize(0), windowMultiplier(0), freqCount(0), channelNameEnable(1),
        smpMemPos(0), lastSequence(0), windowCorrectionFactor(0),
        lastCalc({0, 0}), nextCalc(0.0), lastResult(),
        dumperPrefix("/tmp/plot/"), dumperEnable(false),
#ifdef DFT_MEM_DUMP
        origSigSync(dumperPrefix + "origSigSync"),
        windowdSigSync(dumperPrefix + "windowdSigSync"),
        ppsSigSync(dumperPrefix + "ppsSigSync"),
        phasorRocof(dumperPrefix + "phasorRocof"),
        phasorPhase(dumperPrefix + "phasorPhase"),
        phasorAmplitude(dumperPrefix + "phasorAmplitude"),
        phasorFreq(dumperPrefix + "phasorFreq"),
#endif
        angleUnitFactor(1), phaseOffset(0.0), frequencyOffset(0.0),
        amplitudeOffset(0.0), rocofOffset(0.0) {
  }

  void prepare() override {
    MultiSignalHook::prepare();

    dumperEnable = logger->level() <= SPDLOG_LEVEL_DEBUG;

    signals->clear();
    for (unsigned i = 0; i < signalIndices.size(); i++) {
      // Add signals
      auto freqSig =
          std::make_shared<Signal>("frequency", "Hz", SignalType::FLOAT);
      auto amplSig =
          std::make_shared<Signal>("amplitude", "V", SignalType::FLOAT);
      auto phaseSig = std::make_shared<Signal>(
          "phase", (angleUnitFactor) ? "rad" : "deg",
          SignalType::FLOAT); //angleUnitFactor==1 means rad
      auto rocofSig =
          std::make_shared<Signal>("rocof", "Hz/s", SignalType::FLOAT);

      if (!freqSig || !amplSig || !phaseSig || !rocofSig)
        throw RuntimeError("Failed to create new signals");

      if (channelNameEnable) {
        auto suffix = fmt::format("_{}", signalNames[i]);

        freqSig->name += suffix;
        amplSig->name += suffix;
        phaseSig->name += suffix;
        rocofSig->name += suffix;
      }

      signals->push_back(freqSig);
      signals->push_back(amplSig);
      signals->push_back(phaseSig);
      signals->push_back(rocofSig);
    }

    // Initialize sample memory
    smpMemoryData.clear();
    for (unsigned i = 0; i < signalIndices.size(); i++) {
      smpMemoryData.emplace_back(windowSize, 0.0);
    }

    smpMemoryTs.clear();
    for (unsigned i = 0; i < windowSize; i++) {
      smpMemoryTs.push_back({0});
    }

    lastResult.clear();
    for (unsigned i = 0; i < windowSize; i++) {
      lastResult.push_back({0, 0, 0, 0});
    }

#ifdef DFT_MEM_DUMP
    // Initialize temporary ppsMemory
    ppsMemory.clear();
    ppsMemory.resize(windowSize, 0.0);
#endif

    // Calculate how much zero padding ist needed for a needed resolution
    windowMultiplier =
        ceil(((double)sampleRate / windowSize) / frequencyResolution);
    if (windowMultiplier > 1 && estType == EstimationType::IpDFT)
      throw RuntimeError("Window multiplyer must be 1 if lpdft is used. Change "
                         "resolution, window_size_factor or frequency range! "
                         "Current window multiplyer factor is {}",
                         windowMultiplier);

    freqCount = ceil((endFreqency - startFrequency) / frequencyResolution) + 1;

    // Initialize matrix of dft coeffients
    matrix.clear();
    for (unsigned i = 0; i < freqCount; i++)
      matrix.emplace_back(windowSize * windowMultiplier, 0.0);

    // Initalize dft results matrix
    results.clear();
    for (unsigned i = 0; i < signalIndices.size(); i++) {
      results.emplace_back(freqCount, 0.0);
      absResults.emplace_back(freqCount, 0.0);
    }

    filterWindowCoefficents.resize(windowSize);

    for (unsigned i = 0; i < freqCount; i++)
      absFrequencies.emplace_back(startFrequency + i * frequencyResolution);

    generateDftMatrix();
    calculateWindow(windowType);

    state = State::PREPARED;
  }

  void parse(json_t *json) override {
    MultiSignalHook::parse(json);
    int ret;
    int windowSizeFactor = 1;

    const char *paddingTypeC = nullptr;
    const char *windowTypeC = nullptr;
    const char *estimateTypeC = nullptr;
    const char *angleUnitC = nullptr;
    const char *timeAlignC = nullptr;

    json_error_t err;

    assert(state != State::STARTED);

    Hook::parse(json);

    ret = json_unpack_ex(
        json, &err, 0,
        "{ s?: i, s?: F, s?: F, s?: F, s?: i, s?: i, s?: s, s?: s, s?: s, s?: "
        "i, s?: s, s?: b, s?: s, s?: F, s?: F, s?: F, s?: F}",
        "sample_rate", &sampleRate, "start_freqency", &startFrequency,
        "end_freqency", &endFreqency, "frequency_resolution",
        &frequencyResolution, "dft_rate", &rate, "window_size_factor",
        &windowSizeFactor, "window_type", &windowTypeC, "padding_type",
        &paddingTypeC, "estimate_type", &estimateTypeC, "pps_index", &ppsIndex,
        "angle_unit", &angleUnitC, "add_channel_name", &channelNameEnable,
        "timestamp_align", &timeAlignC, "phase_offset", &phaseOffset,
        "amplitude_offset", &amplitudeOffset, "frequency_offset",
        &frequencyOffset, "rocof_offset", &rocofOffset);
    if (ret)
      throw ConfigError(json, err, "node-config-hook-dft");

    windowSize = sampleRate * windowSizeFactor / (double)rate;
    logger->info(
        "Set windows size to {} samples which fits {} times the rate {}s",
        windowSize, windowSizeFactor, 1.0 / rate);

    if (!windowTypeC)
      logger->info("No Window type given, assume no windowing");
    else if (strcmp(windowTypeC, "flattop") == 0)
      windowType = WindowType::FLATTOP;
    else if (strcmp(windowTypeC, "hamming") == 0)
      windowType = WindowType::HAMMING;
    else if (strcmp(windowTypeC, "hann") == 0)
      windowType = WindowType::HANN;
    else
      throw ConfigError(json, "node-config-hook-dft-window-type",
                        "Invalid window type: {}", windowTypeC);

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

    if (!paddingTypeC)
      logger->info("No Padding type given, assume no zeropadding");
    else if (strcmp(paddingTypeC, "zero") == 0)
      paddingType = PaddingType::ZERO;
    else if (strcmp(paddingTypeC, "signal_repeat") == 0)
      paddingType = PaddingType::SIG_REPEAT;
    else
      throw ConfigError(json, "node-config-hook-dft-padding-type",
                        "Padding type {} not recognized", paddingTypeC);

    if (!estimateTypeC) {
      logger->info("No Frequency estimation type given, assume no none");
      estType = EstimationType::NONE;
    } else if (strcmp(estimateTypeC, "quadratic") == 0)
      estType = EstimationType::QUADRATIC;
    else if (strcmp(estimateTypeC, "ipdft") == 0)
      estType = EstimationType::IpDFT;
    state = State::PARSED;
  }

  void check() override {
    assert(state == State::PARSED);

    if (endFreqency < 0 || endFreqency > sampleRate)
      throw RuntimeError("End frequency must be smaller than sampleRate {}",
                         sampleRate);

    if (frequencyResolution > (double)sampleRate / windowSize)
      throw RuntimeError("The maximum frequency resolution with smaple_rate:{} "
                         "and window_site:{} is {}",
                         sampleRate, windowSize,
                         ((double)sampleRate / windowSize));

    state = State::CHECKED;
  }

  Hook::Reason process(struct Sample *smp) override {
    assert(state == State::STARTED);

    // Update sample memory
    unsigned i = 0;
    for (auto index : signalIndices) {
      smpMemoryData[i++][smpMemPos % windowSize] = smp->data[index].f;
    }
    smpMemoryTs[smpMemPos % windowSize] = smp->ts.origin;

#ifdef DFT_MEM_DUMP
    ppsMemory[smpMemPos % windowSize] = smp->data[ppsIndex].f;
#endif

    bool run = false;
    double smpNsec = smp->ts.origin.tv_sec * 1e9 + smp->ts.origin.tv_nsec;

    if (smpNsec > nextCalc) {
      run = true;
      nextCalc =
          (smp->ts.origin.tv_sec + (((calcCount % rate) + 1) / (double)rate)) *
          1e9;
    }

    if (run) {
      lastCalc = smp->ts.origin;

#ifdef DFT_MEM_DUMP
      double tmpPPSWindow[windowSize];

      for (unsigned i = 0; i < windowSize; i++)
        tmpPPSWindow[i] = ppsMemory[(i + smpMemPos) % windowSize];

      if (dumperEnable)
        ppsSigSync.writeDataBinary(windowSize, tmpPPSWindow);
#endif

#pragma omp parallel for
      for (unsigned i = 0; i < signalIndices.size(); i++) {
        Phasor currentResult = {0, 0, 0, 0};

        calculateDft(PaddingType::ZERO, smpMemoryData[i], results[i],
                     smpMemPos);

        unsigned maxPos = 0;
        double absAmplitude = 0;

        for (unsigned j = 0; j < freqCount; j++) {
          if (absAmplitude < abs(results[i][j])) {
            absAmplitude = abs(results[i][j]);
            maxPos = j;
          }
        }

        int multiplier =
            paddingType == PaddingType::ZERO ? 1 : windowMultiplier;

        DftEstimate dftEstimate = {0};

        if ((maxPos < 1 || maxPos >= freqCount - 1) &&
            estType != EstimationType::NONE) {
          logger->warn("Maximum frequency bin lies on window boundary. Using "
                       "non-estimated results!");
          dftEstimate = noEstimation(
              {0}, {absFrequencies[maxPos + 0], results[i][maxPos + 0]}, {0},
              maxPos, startFrequency, frequencyResolution, multiplier,
              windowSize, windowCorrectionFactor);
        } else {
          Point a = {absFrequencies[maxPos - 1], results[i][maxPos - 1]};
          Point b = {absFrequencies[maxPos + 0], results[i][maxPos + 0]};
          Point c = {absFrequencies[maxPos + 1], results[i][maxPos + 1]};

          if (estType == EstimationType::QUADRATIC)
            dftEstimate = quadraticEstimation(
                a, b, c, maxPos, startFrequency, frequencyResolution,
                multiplier, windowSize, windowCorrectionFactor);
          else if (estType == EstimationType::IpDFT)
            dftEstimate = lpdftEstimation(a, b, c, maxPos, startFrequency,
                                          frequencyResolution, multiplier,
                                          windowSize, windowCorrectionFactor);
          else
            dftEstimate = noEstimation({0}, b, {0}, maxPos, startFrequency,
                                       frequencyResolution, multiplier,
                                       windowSize, windowCorrectionFactor);
        }
        currentResult.frequency = dftEstimate.frequency;
        currentResult.amplitude = dftEstimate.amplitude;
        currentResult.phase =
            dftEstimate.phase * angleUnitFactor; //convert phase from rad to deg

        if (windowSize <= smpMemPos) {

          smp->data[i * 4 + 0].f =
              currentResult.frequency + frequencyOffset; // Frequency
          smp->data[i * 4 + 1].f = (currentResult.amplitude / pow(2, 0.5)) +
                                   amplitudeOffset; // Amplitude
          smp->data[i * 4 + 2].f = currentResult.phase + phaseOffset; // Phase
          smp->data[i * 4 + 3].f =
              ((currentResult.frequency - lastResult[i].frequency) *
               (double)rate) +
              rocofOffset; /* ROCOF */
          ;
          lastResult[i] = currentResult;
        }
      }
#ifdef DFT_MEM_DUMP
      // The following is a debug output and currently only for channel 0
      if (dumperEnable && windowSize * 5 < smpMemPos) {
        phasorFreq.writeDataBinary(1, &(smp->data[0 * 4 + 0].f));
        phasorPhase.writeDataBinary(1, &(smp->data[0 * 4 + 2].f));
        phasorAmplitude.writeDataBinary(1, &(smp->data[0 * 4 + 1].f));
        phasorRocof.writeDataBinary(1, &(smp->data[0 * 4 + 3].f));
      }
#endif

      smp->length = windowSize < smpMemPos ? signalIndices.size() * 4 : 0;

      if (smpMemPos >= windowSize) {
        unsigned tsPos = 0;
        if (timeAlignType == TimeAlign::RIGHT)
          tsPos = smpMemPos;
        else if (timeAlignType == TimeAlign::LEFT)
          tsPos = smpMemPos - windowSize;
        else if (timeAlignType == TimeAlign::CENTER) {
          tsPos = smpMemPos - (windowSize / 2);
        }

        smp->ts.origin = smpMemoryTs[tsPos % windowSize];
      }

      calcCount++;
    }

    if (smp->sequence - lastSequence > 1)
      logger->warn("Calculation is not Realtime. {} sampled missed",
                   smp->sequence - lastSequence);

    lastSequence = smp->sequence;

    if (smpMemPos >=
        2 * windowSize) { //reset smpMemPos if greater than twice the window. Important to handle init
      smpMemPos = windowSize;
    }

    smpMemPos++;

    if (run && windowSize < smpMemPos)
      return Reason::OK;

    return Reason::SKIP_SAMPLE;
  }

  /*
   * This function generates the furie coeffients for the calculateDft function
   */
  void generateDftMatrix() {
    using namespace std::complex_literals;

    omega = exp((-2i * M_PI) / (double)(windowSize * windowMultiplier));
    unsigned startBin = floor(startFrequency / frequencyResolution);

    for (unsigned i = 0; i < freqCount; i++) {
      for (unsigned j = 0; j < windowSize * windowMultiplier; j++)
        matrix[i][j] = pow(omega, (i + startBin) * j);
    }
  }

  /*
   * This function calculates the discrete furie transform of the input signal
   */
  void calculateDft(enum PaddingType padding, std::vector<double> &ringBuffer,
                    std::vector<std::complex<double>> &results,
                    unsigned ringBufferPos) {
    /* RingBuffer size needs to be equal to windowSize
     * prepare sample window The following parts can be combined */
    double tmpSmpWindow[windowSize];

    for (unsigned i = 0; i < windowSize; i++)
      tmpSmpWindow[i] = ringBuffer[(i + ringBufferPos) % windowSize] *
                        filterWindowCoefficents[i];

#ifdef DFT_MEM_DUMP
    if (dumperEnable)
      origSigSync.writeDataBinary(windowSize, tmpSmpWindow);
#endif

    for (unsigned i = 0; i < freqCount; i++) {
      results[i] = 0;

      for (unsigned j = 0; j < windowSize * windowMultiplier; j++) {
        if (padding == PaddingType::ZERO) {
          if (j < windowSize)
            results[i] += tmpSmpWindow[j] * matrix[i][j];
          else
            results[i] += 0;
        } else if (padding == PaddingType::SIG_REPEAT) // Repeat samples
          results[i] += tmpSmpWindow[j % windowSize] * matrix[i][j];
      }
    }
  }

  /*
   * This function prepares the selected window coefficents
   */
  void calculateWindow(enum WindowType windowTypeIn) {
    switch (windowTypeIn) {
    case WindowType::FLATTOP:
      for (unsigned i = 0; i < windowSize; i++) {
        filterWindowCoefficents[i] =
            0.21557895 - 0.41663158 * cos(2 * M_PI * i / (windowSize)) +
            0.277263158 * cos(4 * M_PI * i / (windowSize)) -
            0.083578947 * cos(6 * M_PI * i / (windowSize)) +
            0.006947368 * cos(8 * M_PI * i / (windowSize));
        windowCorrectionFactor += filterWindowCoefficents[i];
      }
      break;

    case WindowType::HAMMING:
    case WindowType::HANN: {
      double a0 = 0.5; // This is the hann window
      if (windowTypeIn == WindowType::HAMMING)
        a0 = 25. / 46;

      for (unsigned i = 0; i < windowSize; i++) {
        filterWindowCoefficents[i] =
            a0 - (1 - a0) * cos(2 * M_PI * i / (windowSize));
        windowCorrectionFactor += filterWindowCoefficents[i];
      }

      break;
    }

    default:
      for (unsigned i = 0; i < windowSize; i++) {
        filterWindowCoefficents[i] = 1;
        windowCorrectionFactor += filterWindowCoefficents[i];
      }
      break;
    }

    windowCorrectionFactor /= windowSize;
  }

  DftEstimate noEstimation(const Point &a, const Point &b, const Point &c,
                           unsigned maxFBin, double startFrequency,
                           double frequencyResolution, double multiplier,
                           double windowSize, double windowCorrectionFactor) {
    // Frequency estimation
    double f_est = startFrequency + maxFBin * frequencyResolution;

    // Amplitude estimation
    double a_est =
        abs(b.y) * 2 / (windowSize * windowCorrectionFactor * multiplier);

    //Phase estimation
    double phase_est = atan2(b.y.imag(), b.y.real());

    return {a_est, f_est, phase_est};
  }

  /*
   * This function is calculation the IpDFT based on the following paper:
   *
   * https://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=7980868&tag=1
   *
   */
  DftEstimate lpdftEstimation(const Point &a, const Point &b, const Point &c,
                              unsigned maxFBin, double startFrequency,
                              double frequencyResolution, double multiplier,
                              double windowSize,
                              double windowCorrectionFactor) {
    double delta = 0;

    //paper eq 8
    if (abs(c.y) > abs(a.y)) {
      delta = 1. * (2. * abs(c.y) - abs(b.y)) / (abs(b.y) + abs(c.y));
    } else {
      delta = -1. * (2. * abs(a.y) - abs(b.y)) / (abs(b.y) + abs(a.y));
    }

    // Frequency estimation (eq 4)
    double f_est =
        startFrequency + ((double)maxFBin + delta) * frequencyResolution;

    // Amplitude estimation (eq 9)
    double a_est = abs(b.y) * abs((M_PI * delta) / sin(M_PI * delta)) *
                   abs(pow(delta, 2) - 1);
    a_est *= 2 / (windowSize * windowCorrectionFactor * multiplier);

    //Phase estimation (eq 10)
    double phase_est = atan2(b.y.imag(), b.y.real()) - M_PI * delta;

    return {a_est, f_est, phase_est};
  }

  /*
   * This function is calculating the mximum based on a quadratic interpolation
   *
   * This function is based on the following paper:
   * https://mgasior.web.cern.ch/pap/biw2004.pdf (equation 10) (freq estimation)
   * https://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak/
   */
  DftEstimate quadraticEstimation(const Point &a, const Point &b,
                                  const Point &c, unsigned maxFBin,
                                  double startFrequency,
                                  double frequencyResolution, double multiplier,
                                  double windowSize,
                                  double windowCorrectionFactor) {
    using namespace std::complex_literals;

    double ay_abs =
        abs(a.y) * 2 / (windowSize * windowCorrectionFactor * multiplier);
    double by_abs =
        abs(b.y) * 2 / (windowSize * windowCorrectionFactor * multiplier);
    double cy_abs =
        abs(c.y) * 2 / (windowSize * windowCorrectionFactor * multiplier);

    // Frequency estimation
    double maxBinEst = (double)maxFBin +
                       (cy_abs - ay_abs) / (2 * (2 * by_abs - ay_abs - cy_abs));
    double f_est = startFrequency +
                   maxBinEst * frequencyResolution; // convert bin to frequency

    // Amplitude estimation
    double f = (a.x * (by_abs - cy_abs) + b.x * (cy_abs - ay_abs) +
                c.x * (ay_abs - by_abs)) /
               ((a.x - b.x) * (a.x - c.x) * (c.x - b.x));
    double g =
        (pow(a.x, 2) * (by_abs - cy_abs) + pow(b.x, 2) * (cy_abs - ay_abs) +
         pow(c.x, 2) * (ay_abs - by_abs)) /
        ((a.x - b.x) * (a.x - c.x) * (b.x - c.x));
    double h = (pow(a.x, 2) * (b.x * cy_abs - c.x * by_abs) +
                a.x * (pow(c.x, 2) * by_abs - pow(b.x, 2) * cy_abs) +
                b.x * c.x * ay_abs * (b.x - c.x)) /
               ((a.x - b.x) * (a.x - c.x) * (b.x - c.x));
    double a_est = f * pow(f_est, 2) + g * f_est + h;

    //Phase estimation
    double phase_est = atan2(b.y.imag(), b.y.real());

    return {a_est, f_est, phase_est};
  }
};

// Register hook
static char n[] = "pmu_dft";
static char d[] = "This hook calculates the  dft on a window";
static HookPlugin<PmuDftHook, n, d,
                  (int)Hook::Flags::NODE_READ | (int)Hook::Flags::NODE_WRITE |
                      (int)Hook::Flags::PATH>
    p;

} // namespace node
} // namespace villas
