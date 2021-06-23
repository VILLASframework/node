/** DFT hook.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include <cstring>
#include <cinttypes>
#include <complex>
#include <vector>
#include <villas/timing.h>

#include <villas/dumper.hpp>
#include <villas/hook.hpp>
#include <villas/path.h>
#include <villas/sample.h>

/* Uncomment to enable dumper of memory windows */
//#define DFT_MEM_DUMP

namespace villas {
namespace node {

class DftHook : public Hook {

protected:
	enum class PaddingType {
		ZERO,
		SIG_REPEAT
	};

	enum class WindowType {
		NONE,
		FLATTOP,
		HANN,
		HAMMING
	};

	enum class FreqEstimationType {
		NONE,
		QUADRATIC
	};

	struct Point {
		double x;
		double y;
	};

	struct Phasor {
		double frequency;
		double amplitude;
		double phase;
		double rocof;	/**< Rate of change of frequency. */
	};

	enum WindowType windowType;
	enum PaddingType paddingType;
	enum FreqEstimationType freqEstType;

	std::vector<std::vector<double>> smpMemory;
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
	unsigned windowMultiplier;	/**< Multiplyer for the window to achieve frequency resolution */
	unsigned freqCount;		/**< Number of requency bins that are calculated */
	bool sync;

	uint64_t smpMemPos;
	uint64_t lastSequence;

	std::complex<double> omega;

	double windowCorrectionFactor;
	struct timespec lastCalc;
	double nextCalc;

	std::vector<int> signalIndex;	/**< A list of signalIndex to do dft on */
	Phasor lastResult;

	std::string dumperPrefix;
	bool dumperEnable;
#ifdef DFT_MEM_DUMP
	Dumper origSigSync;
	Dumper windowdSigSync;
	Dumper ppsSigSync;
#endif
	Dumper phasorRocof;
	Dumper phasorPhase;
	Dumper phasorAmplitude;
	Dumper phasorFreq;

public:
	DftHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		windowType(WindowType::NONE),
		paddingType(PaddingType::ZERO),
		freqEstType(FreqEstimationType::NONE),
		smpMemory(),
#ifdef DFT_MEM_DUMP
		ppsMemory(),
#endif
		matrix(),
		results(),
		filterWindowCoefficents(),
		absResults(),
		absFrequencies(),
		calcCount(0),
		sampleRate(0),
		startFrequency(0),
		endFreqency(0),
		frequencyResolution(0),
		rate(0),
		ppsIndex(0),
		windowSize(0),
		windowMultiplier(0),
		freqCount(0),
		sync(0),
		smpMemPos(0),
		lastSequence(0),
		windowCorrectionFactor(0),
		lastCalc({0, 0}),
		nextCalc(0.0),
		signalIndex(),
		lastResult({0,0,0,0}),
		dumperPrefix("/tmp/plot/"),
		dumperEnable(logger->level() <= SPDLOG_LEVEL_DEBUG),
#ifdef DFT_MEM_DUMP
		origSigSync(dumperPrefix + "origSigSync"),
		windowdSigSync(dumperPrefix + "windowdSigSync"),
		ppsSigSync(dumperPrefix + "ppsSigSync"),
#endif
		phasorRocof(dumperPrefix + "phasorRocof"),
		phasorPhase(dumperPrefix + "phasorPhase"),
		phasorAmplitude(dumperPrefix + "phasorAmplitude"),
		phasorFreq(dumperPrefix + "phasorFreq")
	{ }

	virtual void prepare()
	{
		signal_list_clear(&signals);
		for (unsigned i = 0; i < signalIndex.size(); i++) {
			struct signal *freqSig;
			struct signal *amplSig;
			struct signal *phaseSig;
			struct signal *rocofSig;

			/* Add signals */
			freqSig = signal_create("amplitude", "V", SignalType::FLOAT);
			amplSig = signal_create("phase", "rad", SignalType::FLOAT);
			phaseSig = signal_create("frequency", "Hz", SignalType::FLOAT);
			rocofSig = signal_create("rocof", "Hz/s", SignalType::FLOAT);

			if (!freqSig || !amplSig || !phaseSig || !rocofSig)
				throw RuntimeError("Failed to create new signals");

			vlist_push(&signals, freqSig);
			vlist_push(&signals, amplSig);
			vlist_push(&signals, phaseSig);
			vlist_push(&signals, rocofSig);
		}

		/* Initialize sample memory */
		smpMemory.clear();
		for (unsigned i = 0; i < signalIndex.size(); i++)
			smpMemory.emplace_back(windowSize, 0.0);

#ifdef DFT_MEM_DUMP
		/* Initialize temporary ppsMemory */
		ppsMemory.clear();
		ppsMemory.resize(windowSize, 0.0);
#endif

		//temporary ppsMemory
		ppsMemory.clear();
		ppsMemory.resize(windowSize, 0.0);

		/* Calculate how much zero padding ist needed for a needed resolution */
		windowMultiplier = ceil(((double) sampleRate / windowSize) / frequencyResolution);

		freqCount = ceil((endFreqency - startFrequency) / frequencyResolution) + 1;

		/* Initialize matrix of dft coeffients */
		matrix.clear();
		for (unsigned i = 0; i < freqCount; i++)
			matrix.emplace_back(windowSize * windowMultiplier, 0.0);

		/* Initalize dft results matrix */
		results.clear();
		for (unsigned i = 0; i < signalIndex.size(); i++){
			results.emplace_back(freqCount, 0.0);
			absResults.emplace_back(freqCount, 0.0);
		}

		filterWindowCoefficents.resize(windowSize);

<<<<<<< HEAD
		for (unsigned i = 0; i < freqCount; i++) {
			absFrequencies.emplace_back(startFrequency + i * frequencyResolution);
=======
		if (logger->level() <= SPDLOG_LEVEL_DEBUG) {
#ifdef DFT_MEM_DUMP
			//origSigSync = std::make_shared<Dumper>("/tmp/plot/origSigSync");
			//windowdSigSync = std::make_shared<Dumper>("/tmp/plot/windowdSigSync");
			//ppsSigSync = std::make_shared<Dumper>("/tmp/plot/ppsSigSync");
#endif
			phasorPhase = std::make_shared<Dumper>("/tmp/plot/phasorPhase");
			phasorAmplitude = std::make_shared<Dumper>("/tmp/plot/phasorAmplitude");
			phasorFreq = std::make_shared<Dumper>("/tmp/plot/phasorFreq");
			ppsSigSync = std::make_shared<Dumper>("/tmp/plot/ppsSigSync");
>>>>>>> dft: ifdef for full mem dump of dft
		}

		generateDftMatrix();
		calculateWindow(windowType);

		state = State::PREPARED;
	}

	virtual void parse(json_t *json)
	{
		int ret;
		int windowSizeFactor = 1;

		const char *paddingTypeC = nullptr;
		const char *windowTypeC = nullptr;
		const char *freqEstimateTypeC = nullptr;

		json_error_t err;
		json_t *jsonChannelList = nullptr;

		assert(state != State::STARTED);

		Hook::parse(json);

		ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: F, s?: F, s?: F, s?: i , s?: i, s?: s, s?: s, s?: s, s?: b, s?: o }",
			"sample_rate", &sampleRate,
			"start_freqency", &startFrequency,
			"end_freqency", &endFreqency,
			"frequency_resolution", &frequencyResolution,
			"dft_rate", &rate,
			"window_size_factor", &windowSizeFactor,
			"window_type", &windowTypeC,
			"padding_type", &paddingTypeC,
			"freq_estimate_type", &freqEstimateTypeC,
			"sync", &sync,
			"signal_index", &jsonChannelList,
			"pps_index", &ppsIndex
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-dft");

		if (jsonChannelList != nullptr) {
			signalIndex.clear();
			if (jsonChannelList->type == JSON_ARRAY) {
				size_t i;
				json_t *jsonValue;
				json_array_foreach(jsonChannelList, i, jsonValue) {
					if (!json_is_number(jsonValue))
						throw ConfigError(jsonValue, "node-config-hook-dft-channel", "Values must be given as array of integer values!");

					auto idx = json_number_value(jsonValue);

					signalIndex.push_back(idx);
				}
			}
			else if (jsonChannelList->type == JSON_INTEGER) {
				if (!json_is_number(jsonChannelList))
					throw ConfigError(jsonChannelList, "node-config-hook-dft-channel", "Value must be given as integer value!");

				auto idx = json_number_value(jsonChannelList);

				signalIndex.push_back(idx);
			}
			else
				throw ConfigError(jsonChannelList, "node-config-hook-dft-signal-index", "Could not parse channel list.");
		}
		else
			throw ConfigError(jsonChannelList, "node-config-node-signal", "No parameter signalIndex given.");

		windowSize = sampleRate * windowSizeFactor / (double) rate;
		logger->debug("Set windows size to {} samples which fits 1 / rate {}s", windowSize, 1.0 / rate);

		if (!windowTypeC)
			logger->info("No Window type given, assume no windowing");
		else if (strcmp(windowTypeC, "flattop") == 0)
			windowType = WindowType::FLATTOP;
		else if (strcmp(windowTypeC, "hamming") == 0)
			windowType = WindowType::HAMMING;
		else if (strcmp(windowTypeC, "hann") == 0)
			windowType = WindowType::HANN;
		else
			throw ConfigError(json, "node-config-hook-dft-window-type", "Invalid window type: {}", windowTypeC);

		if (!paddingTypeC)
			logger->info("No Padding type given, assume no zeropadding");
		else if (strcmp(paddingTypeC, "signal_repeat") == 0)
			paddingType = PaddingType::SIG_REPEAT;
		else
			throw ConfigError(json, "node-config-hook-dft-padding-type", "Padding type {} not recognized", paddingTypeC);

		if (!freqEstimateTypeC) {
			logger->info("No Frequency estimation type given, assume no none");
			freqEstType = FreqEstimationType::NONE;
		}
		else if (strcmp(freqEstimateTypeC, "quadratic") == 0)
			freqEstType = FreqEstimationType::QUADRATIC;

		state = State::PARSED;
	}

	virtual void check()
	{
		assert(state == State::PARSED);

		if (endFreqency < 0 || endFreqency > sampleRate)
			throw RuntimeError("End frequency must be smaller than sampleRate {}", sampleRate);

		if (frequencyResolution > (double) sampleRate / windowSize)
			throw RuntimeError("The maximum frequency resolution with smaple_rate:{} and window_site:{} is {}", sampleRate, windowSize, ((double)sampleRate/windowSize));

		state = State::CHECKED;
	}

	virtual Hook::Reason process(struct sample *smp)
	{
		assert(state == State::STARTED);

		for (unsigned i = 0; i < signalIndex.size(); i++)
			smpMemory[i][smpMemPos % windowSize] = smp->data[signalIndex[i]].f;

#ifdef DFT_MEM_DUMP
		ppsMemory[smpMemPos % windowSize] = smp->data[ppsIndex].f;
#endif
		smpMemPos++;

		bool run = false;
		if (sync) {
			double smpNsec = smp->ts.origin.tv_sec * 1e9 + smp->ts.origin.tv_nsec;

			if (smpNsec > nextCalc) {
				run = true;
				nextCalc = (smp->ts.origin.tv_sec + (((calcCount % rate) + 1) / (double) rate)) * 1e9;
			}
		}

		if (run) {
			lastCalc = smp->ts.origin;

#ifdef DFT_MEM_DUMP
			double tmpPPSWindow[windowSize];

			for (unsigned i = 0; i< windowSize; i++)
				tmpPPSWindow[i] = ppsMemory[(i + smpMemPos) % windowSize];

			if (dumperEnable)
				ppsSigSync.writeDataBinary(windowSize, tmpPPSWindow);
#endif

			#pragma omp parallel for
			for (unsigned i = 0; i < signalIndex.size(); i++) {
				Phasor currentResult = {0,0,0,0};

				calculateDft(PaddingType::ZERO, smpMemory[i], results[i], smpMemPos);

				unsigned maxPos = 0;

				for (unsigned j = 0; j < freqCount; j++) {
					int multiplier = paddingType == PaddingType::ZERO
						? 1
						: windowMultiplier;
					absResults[i][j] = abs(results[i][j]) * 2 / (windowSize * windowCorrectionFactor * multiplier);
					if (currentResult.amplitude < absResults[i][j]) {
						currentResult.frequency = absFrequencies[j];
						currentResult.amplitude = absResults[i][j];
						maxPos = j;
					}
				}

				if (freqEstType == FreqEstimationType::QUADRATIC) {
					if (maxPos < 1 || maxPos >= freqCount - 1)
						logger->warn("Maximum frequency bin lies on window boundary. Using non-estimated results!");
					else {
						Point a = { absFrequencies[maxPos - 1], absResults[i][maxPos - 1] };
						Point b = { absFrequencies[maxPos + 0], absResults[i][maxPos + 0] };
						Point c = { absFrequencies[maxPos + 1], absResults[i][maxPos + 1] };

						Point estimate = quadraticEstimation(a, b, c, maxPos);
						currentResult.frequency = estimate.x;
						currentResult.amplitude = estimate.y;
					}
				}

				if (windowSize < smpMemPos) {

					smp->data[i * 4 + 0].f = currentResult.frequency; /* Frequency */
					smp->data[i * 4 + 1].f = (currentResult.amplitude / pow(2, 0.5)); /* Amplitude */
					smp->data[i * 4 + 2].f = atan2(results[i][maxPos].imag(), results[i][maxPos].real()); /* Phase */
					smp->data[i * 4 + 3].f = (currentResult.frequency - lastResult.frequency) / (double)rate; /* RoCof */

				}

				lastResult = currentResult;
			}

			// The following is a debug output and currently only for channel 0
			if (dumperEnable && windowSize * 5 < smpMemPos){
				phasorFreq.writeDataBinary(1, &(smp->data[0 * 4 + 0].f));
				phasorPhase.writeDataBinary(1, &(smp->data[0 * 4 + 2].f));
				phasorAmplitude.writeDataBinary(1, &(smp->data[0 * 4 + 1].f));
				phasorRocof.writeDataBinary(1, &(smp->data[0 * 4 + 3].f));
			}

			smp->length = windowSize < smpMemPos ? signalIndex.size() * 4 : 0;

			calcCount++;
		}

		if (smp->sequence - lastSequence > 1)
			logger->warn("Calculation is not Realtime. {} sampled missed", smp->sequence - lastSequence);

		lastSequence = smp->sequence;

		if (run && windowSize < smpMemPos)
			return Reason::OK;

		return Reason::SKIP_SAMPLE;
	}

	/**
	 * This function generates the furie coeffients for the calculateDft function/** DFT hook.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include <cstring>
#include <cinttypes>
#include <complex>
#include <vector>
#include <villas/timing.h>

#include <villas/dumper.hpp>
#include <villas/hook.hpp>
#include <villas/path.h>
#include <villas/sample.h>

/* Uncomment to enable dumper of memory windows */
//#define DFT_MEM_DUMP

namespace villas {
namespace node {

class DftHook : public Hook {

protected:
	enum class PaddingType {
		ZERO,
		SIG_REPEAT
	};

	enum class WindowType {
		NONE,
		FLATTOP,
		HANN,
		HAMMING
	};

	enum class FreqEstimationType {
		NONE,
		QUADRATIC
	};

	struct Point {
		double x;
		double y;
	};

	struct Phasor {
		double frequency;
		double amplitude;
		double phase;
		double rocof;	/**< Rate of change of frequency. */
	};

	enum WindowType windowType;
	enum PaddingType paddingType;
	enum FreqEstimationType freqEstType;

	std::vector<std::vector<double>> smpMemory;
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
	unsigned windowMultiplier;	/**< Multiplyer for the window to achieve frequency resolution */
	unsigned freqCount;		/**< Number of requency bins that are calculated */
	bool sync;

	uint64_t smpMemPos;
	uint64_t lastSequence;

	std::complex<double> omega;

	double windowCorrectionFactor;
	struct timespec lastCalc;
	double nextCalc;

	std::vector<int> signalIndex;	/**< A list of signalIndex to do dft on */
	Phasor lastResult;

	std::string dumperPrefix;
	bool dumperEnable;
#ifdef DFT_MEM_DUMP
	Dumper origSigSync;
	Dumper windowdSigSync;
	Dumper ppsSigSync;
#endif
	Dumper phasorRocof;
	Dumper phasorPhase;
	Dumper phasorAmplitude;
	Dumper phasorFreq;

public:
	DftHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		windowType(WindowType::NONE),
		paddingType(PaddingType::ZERO),
		freqEstType(FreqEstimationType::NONE),
		smpMemory(),
#ifdef DFT_MEM_DUMP
		ppsMemory(),
#endif
		matrix(),
		results(),
		filterWindowCoefficents(),
		absResults(),
		absFrequencies(),
		calcCount(0),
		sampleRate(0),
		startFrequency(0),
		endFreqency(0),
		frequencyResolution(0),
		rate(0),
		ppsIndex(0),
		windowSize(0),
		windowMultiplier(0),
		freqCount(0),
		sync(0),
		smpMemPos(0),
		lastSequence(0),
		windowCorrectionFactor(0),
		lastCalc({0, 0}),
		nextCalc(0.0),
		signalIndex(),
		lastResult({0,0,0,0}),
		dumperPrefix("/tmp/plot/"),
		dumperEnable(logger->level() <= SPDLOG_LEVEL_DEBUG),
#ifdef DFT_MEM_DUMP
		origSigSync(dumperPrefix + "origSigSync"),
		windowdSigSync(dumperPrefix + "windowdSigSync"),
		ppsSigSync(dumperPrefix + "ppsSigSync"),
#endif
		phasorRocof(dumperPrefix + "phasorRocof"),
		phasorPhase(dumperPrefix + "phasorPhase"),
		phasorAmplitude(dumperPrefix + "phasorAmplitude"),
		phasorFreq(dumperPrefix + "phasorFreq")
	{ }

	virtual void prepare()
	{
		signal_list_clear(&signals);
		for (unsigned i = 0; i < signalIndex.size(); i++) {
			struct signal *freqSig;
			struct signal *amplSig;
			struct signal *phaseSig;
			struct signal *rocofSig;

			/* Add signals */
			freqSig = signal_create("amplitude", "V", SignalType::FLOAT);
			amplSig = signal_create("phase", "rad", SignalType::FLOAT);
			phaseSig = signal_create("frequency", "Hz", SignalType::FLOAT);
			rocofSig = signal_create("rocof", "Hz/s", SignalType::FLOAT);

			if (!freqSig || !amplSig || !phaseSig || !rocofSig)
				throw RuntimeError("Failed to create new signals");

			vlist_push(&signals, freqSig);
			vlist_push(&signals, amplSig);
			vlist_push(&signals, phaseSig);
			vlist_push(&signals, rocofSig);
		}

		/* Initialize sample memory */
		smpMemory.clear();
		for (unsigned i = 0; i < signalIndex.size(); i++)
			smpMemory.emplace_back(windowSize, 0.0);

#ifdef DFT_MEM_DUMP
		/* Initialize temporary ppsMemory */
		ppsMemory.clear();
		ppsMemory.resize(windowSize, 0.0);
#endif

		/* Calculate how much zero padding ist needed for a needed resolution */
		windowMultiplier = ceil(((double) sampleRate / windowSize) / frequencyResolution);

		freqCount = ceil((endFreqency - startFrequency) / frequencyResolution) + 1;

		/* Initialize matrix of dft coeffients */
		matrix.clear();
		for (unsigned i = 0; i < freqCount; i++)
			matrix.emplace_back(windowSize * windowMultiplier, 0.0);

		/* Initalize dft results matrix */
		results.clear();
		for (unsigned i = 0; i < signalIndex.size(); i++){
			results.emplace_back(freqCount, 0.0);
			absResults.emplace_back(freqCount, 0.0);
		}

		filterWindowCoefficents.resize(windowSize);

		for (unsigned i = 0; i < freqCount; i++) {
			absFrequencies.emplace_back(startFrequency + i * frequencyResolution);
		}

		generateDftMatrix();
		calculateWindow(windowType);

		state = State::PREPARED;
	}

	virtual void parse(json_t *json)
	{
		int ret;
		int windowSizeFactor = 1;

		const char *paddingTypeC = nullptr;
		const char *windowTypeC = nullptr;
		const char *freqEstimateTypeC = nullptr;

		json_error_t err;
		json_t *jsonChannelList = nullptr;

		assert(state != State::STARTED);

		Hook::parse(json);

		ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: F, s?: F, s?: F, s?: i , s?: i, s?: s, s?: s, s?: s, s?: b, s?: o }",
			"sample_rate", &sampleRate,
			"start_freqency", &startFrequency,
			"end_freqency", &endFreqency,
			"frequency_resolution", &frequencyResolution,
			"dft_rate", &rate,
			"window_size_factor", &windowSizeFactor,
			"window_type", &windowTypeC,
			"padding_type", &paddingTypeC,
			"freq_estimate_type", &freqEstimateTypeC,
			"sync", &sync,
			"signal_index", &jsonChannelList,
			"pps_index", &ppsIndex
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-dft");

		if (jsonChannelList != nullptr) {
			signalIndex.clear();
			if (jsonChannelList->type == JSON_ARRAY) {
				size_t i;
				json_t *jsonValue;
				json_array_foreach(jsonChannelList, i, jsonValue) {
					if (!json_is_number(jsonValue))
						throw ConfigError(jsonValue, "node-config-hook-dft-channel", "Values must be given as array of integer values!");

					auto idx = json_number_value(jsonValue);

					signalIndex.push_back(idx);
				}
			}
			else if (jsonChannelList->type == JSON_INTEGER) {
				if (!json_is_number(jsonChannelList))
					throw ConfigError(jsonChannelList, "node-config-hook-dft-channel", "Value must be given as integer value!");

				auto idx = json_number_value(jsonChannelList);

				signalIndex.push_back(idx);
			}
			else
				throw ConfigError(jsonChannelList, "node-config-hook-dft-signal-index", "Could not parse channel list.");
		}
		else
			throw ConfigError(jsonChannelList, "node-config-node-signal", "No parameter signalIndex given.");

		windowSize = sampleRate * windowSizeFactor / (double) rate;
		logger->debug("Set windows size to {} samples which fits 1 / rate {}s", windowSize, 1.0 / rate);

		if (!windowTypeC)
			logger->info("No Window type given, assume no windowing");
		else if (strcmp(windowTypeC, "flattop") == 0)
			windowType = WindowType::FLATTOP;
		else if (strcmp(windowTypeC, "hamming") == 0)
			windowType = WindowType::HAMMING;
		else if (strcmp(windowTypeC, "hann") == 0)
			windowType = WindowType::HANN;
		else
			throw ConfigError(json, "node-config-hook-dft-window-type", "Invalid window type: {}", windowTypeC);

		if (!paddingTypeC)
			logger->info("No Padding type given, assume no zeropadding");
		else if (strcmp(paddingTypeC, "signal_repeat") == 0)
			paddingType = PaddingType::SIG_REPEAT;
		else
			throw ConfigError(json, "node-config-hook-dft-padding-type", "Padding type {} not recognized", paddingTypeC);

		if (!freqEstimateTypeC) {
			logger->info("No Frequency estimation type given, assume no none");
			freqEstType = FreqEstimationType::NONE;
		}
		else if (strcmp(freqEstimateTypeC, "quadratic") == 0)
			freqEstType = FreqEstimationType::QUADRATIC;

		state = State::PARSED;
	}

	virtual void check()
	{
		assert(state == State::PARSED);

		if (!freqEstimateTypeC) {
			logger->info("No Frequency estimation type given, assume no none");
			freqEstType = FreqEstimationType::NONE;
		}
		else if (strcmp(freqEstimateTypeC, "quadratic") == 0)
			freqEstType = FreqEstimationType::QUADRATIC;

		if (endFreqency < 0 || endFreqency > sampleRate)
			throw RuntimeError("End frequency must be smaller than sampleRate {}", sampleRate);

		if (frequencyResolution > (double) sampleRate / windowSize)
			throw RuntimeError("The maximum frequency resolution with smaple_rate:{} and window_site:{} is {}", sampleRate, windowSize, ((double)sampleRate/windowSize));

		state = State::CHECKED;
	}

	virtual Hook::Reason process(struct sample *smp)
	{
		assert(state == State::STARTED);

		for (unsigned i = 0; i < signalIndex.size(); i++)
			smpMemory[i][smpMemPos % windowSize] = smp->data[signalIndex[i]].f;

#ifdef DFT_MEM_DUMP
		ppsMemory[smpMemPos % windowSize] = smp->data[ppsIndex].f;
#endif
		smpMemPos++;

		bool run = false;
		if (sync) {
			double smpNsec = smp->ts.origin.tv_sec * 1e9 + smp->ts.origin.tv_nsec;

			if (smpNsec > nextCalc) {
				run = true;
				nextCalc = (smp->ts.origin.tv_sec + (((calcCount % rate) + 1) / (double) rate)) * 1e9;
			}
		}

		if (run) {
			lastCalc = smp->ts.origin;

#ifdef DFT_MEM_DUMP
			double tmpPPSWindow[windowSize];

			for (unsigned i = 0; i< windowSize; i++)
				tmpPPSWindow[i] = ppsMemory[(i + smpMemPos) % windowSize];

			if (dumperEnable)
				ppsSigSync.writeDataBinary(windowSize, tmpPPSWindow);
#endif

			#pragma omp parallel for
			for (unsigned i = 0; i < signalIndex.size(); i++) {
				Phasor currentResult = {0,0,0,0};

				calculateDft(PaddingType::ZERO, smpMemory[i], results[i], smpMemPos);

				unsigned maxPos = 0;

				for (unsigned j = 0; j < freqCount; j++) {
					int multiplier = paddingType == PaddingType::ZERO
						? 1
						: windowMultiplier;
					absResults[i][j] = abs(results[i][j]) * 2 / (windowSize * windowCorrectionFactor * multiplier);
					if (currentResult.amplitude < absResults[i][j]) {
						currentResult.frequency = absFrequencies[j];
						currentResult.amplitude = absResults[i][j];
						maxPos = j;
					}
				}

				if (freqEstType == FreqEstimationType::QUADRATIC) {
					if (maxPos < 1 || maxPos >= freqCount - 1)
						logger->warn("Maximum frequency bin lies on window boundary. Using non-estimated results!");
					else {
						Point a = { absFrequencies[maxPos - 1], absResults[i][maxPos - 1] };
						Point b = { absFrequencies[maxPos + 0], absResults[i][maxPos + 0] };
						Point c = { absFrequencies[maxPos + 1], absResults[i][maxPos + 1] };

						Point estimate = quadraticEstimation(a, b, c, maxPos);
						currentResult.frequency = estimate.x;
						currentResult.amplitude = estimate.y;
					}
				}

				if (windowSize < smpMemPos) {

					smp->data[i * 4 + 0].f = currentResult.frequency; /* Frequency */
					smp->data[i * 4 + 1].f = (currentResult.amplitude / pow(2, 0.5)); /* Amplitude */
					smp->data[i * 4 + 2].f = atan2(results[i][maxPos].imag(), results[i][maxPos].real()); /* Phase */
					smp->data[i * 4 + 3].f = (currentResult.frequency - lastResult.frequency) / (double)rate; /* RoCof */

				}

				lastResult = currentResult;
			}

			// The following is a debug output and currently only for channel 0
			if (dumperEnable && windowSize * 5 < smpMemPos){
				phasorFreq.writeDataBinary(1, &(smp->data[0 * 4 + 0].f));
				phasorPhase.writeDataBinary(1, &(smp->data[0 * 4 + 2].f));
				phasorAmplitude.writeDataBinary(1, &(smp->data[0 * 4 + 1].f));
				phasorRocof.writeDataBinary(1, &(smp->data[0 * 4 + 3].f));
			}

			smp->length = windowSize < smpMemPos ? signalIndex.size() * 4 : 0;

			calcCount++;
		}

		if (smp->sequence - lastSequence > 1)
			logger->warn("Calculation is not Realtime. {} sampled missed", smp->sequence - lastSequence);

		lastSequence = smp->sequence;

		if (run && windowSize < smpMemPos)
			return Reason::OK;

		return Reason::SKIP_SAMPLE;
	}

	/**
	 * This function generates the furie coeffients for the calculateDft function
	 */
	void generateDftMatrix()
	{
		using namespace std::complex_literals;

		omega = exp((-2i * M_PI) / (double)(windowSize * windowMultiplier));
		unsigned startBin = floor(startFrequency / frequencyResolution);

		for (unsigned i = 0; i <  freqCount ; i++) {
			for (unsigned j = 0 ; j < windowSize * windowMultiplier ; j++)
				matrix[i][j] = pow(omega, (i + startBin) * j);
		}
	}

	/**
	 * This function calculates the discrete furie transform of the input signal
	 */
	void calculateDft(enum PaddingType padding, std::vector<double> &ringBuffer, std::vector<std::complex<double>> &results, unsigned ringBufferPos)
	{
		/* RingBuffer size needs to be equal to windowSize
		 * prepare sample window The following parts can be combined */
		double tmpSmpWindow[windowSize];

		for (unsigned i = 0; i< windowSize; i++)
			tmpSmpWindow[i] = ringBuffer[(i + ringBufferPos) % windowSize];

#ifdef DFT_MEM_DUMP
<<<<<<< HEAD
		if (dumperEnable)
			origSigSync.writeDataBinary(windowSize, tmpSmpWindow);
#endif

		for (unsigned i = 0; i < windowSize; i++)
			tmpSmpWindow[i] *= filterWindowCoefficents[i];

#ifdef DFT_MEM_DUMP
		if (dumperEnable)
			windowdSigSync.writeDataBinary(windowSize, tmpSmpWindow);
#endif

		for (unsigned i = 0; i < freqCount; i++) {
			results[i] = 0;

			for (unsigned j = 0; j < windowSize * windowMultiplier; j++) {
				if (padding == PaddingType::ZERO) {
					if (j < (windowSize))
						results[i] += tmpSmpWindow[j] * matrix[i][j];
					else
						results[i] += 0;
				}
				else if (padding == PaddingType::SIG_REPEAT) /* Repeat samples */
					results[i] += tmpSmpWindow[j % windowSize] * matrix[i][j];
			}
		}
	}

	/**
	 * This function prepares the selected window coefficents
	 */
	void calculateWindow(enum WindowType windowTypeIn)
	{
		switch (windowTypeIn) {
			case WindowType::FLATTOP:
				for (unsigned i = 0; i < windowSize; i++) {
					filterWindowCoefficents[i] = 0.21557895
									- 0.41663158  * cos(2 * M_PI * i / (windowSize))
									+ 0.277263158 * cos(4 * M_PI * i / (windowSize))
									- 0.083578947 * cos(6 * M_PI * i / (windowSize))
									+ 0.006947368 * cos(8 * M_PI * i / (windowSize));
					windowCorrectionFactor += filterWindowCoefficents[i];
				}
				break;

			case WindowType::HAMMING:
			case WindowType::HANN: {
				double a0 = 0.5; /* This is the hann window */
				if (windowTypeIn == WindowType::HAMMING)
					a0 = 25./46;

				for (unsigned i = 0; i < windowSize; i++) {
					filterWindowCoefficents[i] = a0 - (1 - a0) * cos(2 * M_PI * i / (windowSize));
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

	/**
	 * This function is calculating the mximum based on a quadratic interpolation
	 *
	 * This function is based on the following paper:
	 * https://mgasior.web.cern.ch/pap/biw2004.pdf
	 * https://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak/
	 * 	 *
	 * In particular equation 10
	 */
	Point quadraticEstimation(const Point &a, const Point &b, const Point &c, unsigned maxFBin)
	{
		// Frequency estimation
		double maxBinEst = (double) maxFBin + (c.y - a.y) / (2 * (2 * b.y - a.y - c.y));
		double y_Fmax = startFrequency + maxBinEst * frequencyResolution; // convert bin to frequency

		// Amplitude estimation
		double f = (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y)) / ((a.x - b.x) * (a.x - c.x) * (c.x - b.x));
		double g = (pow(a.x, 2) * (b.y - c.y) + pow(b.x, 2) * (c.y - a.y) + pow(c.x, 2) * (a.y - b.y)) / ((a.x - b.x) * (a.x - c.x) * (b.x - c.x));
		double h = (pow(a.x, 2) * (b.x * c.y - c.x * b.y) + a.x * (pow(c.x, 2) * b.y - pow(b.x,2) * c.y)+ b.x * c.x * a.y * (b.x - c.x)) / ((a.x - b.x) * (a.x - c.x) * (b.x - c.x));
		double i = f * pow(y_Fmax,2) + g * y_Fmax + h;

		return { y_Fmax, i };
	}
};

/* Register hook */
static char n[] = "dft";
static char d[] = "This hook calculates the  dft on a window";
static HookPlugin<DftHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */

		omega = exp((-2i * M_PI) / (double)(windowSize * windowMultiplier));
		unsigned startBin = floor(startFrequency / frequencyResolution);

		for (unsigned i = 0; i <  freqCount ; i++) {
			for (unsigned j = 0 ; j < windowSize * windowMultiplier ; j++)
				matrix[i][j] = pow(omega, (i + startBin) * j);
		}
	}

	/**
	 * This function calculates the discrete furie transform of the input signal
	 */
	void calculateDft(enum PaddingType padding, std::vector<double> &ringBuffer, std::vector<std::complex<double>> &results, unsigned ringBufferPos)
	{
		/* RingBuffer size needs to be equal to windowSize
		 * prepare sample window The following parts can be combined */
		double tmpSmpWindow[windowSize];

		for (unsigned i = 0; i< windowSize; i++)
			tmpSmpWindow[i] = ringBuffer[(i + ringBufferPos) % windowSize];

#ifdef DFT_MEM_DUMP
		if (dumperEnable)
			origSigSync.writeDataBinary(windowSize, tmpSmpWindow);
#endif

		for (unsigned i = 0; i < windowSize; i++)
			tmpSmpWindow[i] *= filterWindowCoefficents[i];

#ifdef DFT_MEM_DUMP
		if (dumperEnable)
			windowdSigSync.writeDataBinary(windowSize, tmpSmpWindow);
#endif
=======

		if (origSigSync)
			origSigSync->writeData(windowSize, tmpSmpWindow);

		if (dftCalcCount > 1 && phasorAmplitude)
			phasorAmplitude->writeData(1, &tmpSmpWindow[windowSize - 1]);

#endif

			tmpSmpWindow[i] *= filterWindowCoefficents[i];

#ifdef DFT_MEM_DUMP

		if (windowdSigSync)
			windowdSigSync->writeData(windowSize, tmpSmpWindow);
>>>>>>> dft: ifdef for full mem dump of dft

#endif

		for (unsigned i = 0; i < freqCount; i++) {
			results[i] = 0;

			for (unsigned j = 0; j < windowSize * windowMultiplier; j++) {
				if (padding == PaddingType::ZERO) {
					if (j < (windowSize))
						results[i] += tmpSmpWindow[j] * matrix[i][j];
					else
						results[i] += 0;
				}
				else if (padding == PaddingType::SIG_REPEAT) /* Repeat samples */
					results[i] += tmpSmpWindow[j % windowSize] * matrix[i][j];
			}
		}
	}

	/**
	 * This function prepares the selected window coefficents
	 */
	void calculateWindow(enum WindowType windowTypeIn)
	{
		switch (windowTypeIn) {
			case WindowType::FLATTOP:
				for (unsigned i = 0; i < windowSize; i++) {
					filterWindowCoefficents[i] = 0.21557895
									- 0.41663158  * cos(2 * M_PI * i / (windowSize))
									+ 0.277263158 * cos(4 * M_PI * i / (windowSize))
									- 0.083578947 * cos(6 * M_PI * i / (windowSize))
									+ 0.006947368 * cos(8 * M_PI * i / (windowSize));
					windowCorrectionFactor += filterWindowCoefficents[i];
				}
				break;

			case WindowType::HAMMING:
			case WindowType::HANN: {
				double a0 = 0.5; /* This is the hann window */
				if (windowTypeIn == WindowType::HAMMING)
					a0 = 25./46;

				for (unsigned i = 0; i < windowSize; i++) {
					filterWindowCoefficents[i] = a0 - (1 - a0) * cos(2 * M_PI * i / (windowSize));
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

	/**
	 * This function is calculating the mximum based on a quadratic interpolation
	 *
	 * This function is based on the following paper:
	 * https://mgasior.web.cern.ch/pap/biw2004.pdf
	 * https://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak/
	 * 	 *
	 * In particular equation 10
	 */
	Point quadraticEstimation(const Point &a, const Point &b, const Point &c, unsigned maxFBin)
	{
		// Frequency estimation
		double maxBinEst = (double) maxFBin + (c.y - a.y) / (2 * (2 * b.y - a.y - c.y));
		double y_Fmax = startFrequency + maxBinEst * frequencyResolution; // convert bin to frequency

		// Amplitude estimation
		double f = (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y)) / ((a.x - b.x) * (a.x - c.x) * (c.x - b.x));
		double g = (pow(a.x, 2) * (b.y - c.y) + pow(b.x, 2) * (c.y - a.y) + pow(c.x, 2) * (a.y - b.y)) / ((a.x - b.x) * (a.x - c.x) * (b.x - c.x));
		double h = (pow(a.x, 2) * (b.x * c.y - c.x * b.y) + a.x * (pow(c.x, 2) * b.y - pow(b.x,2) * c.y)+ b.x * c.x * a.y * (b.x - c.x)) / ((a.x - b.x) * (a.x - c.x) * (b.x - c.x));
		double i = f * pow(y_Fmax,2) + g * y_Fmax + h;

		return { y_Fmax, i };
	}

	/**
	 * This function is calculating the mximum based on a quadratic interpolation
	 * 
	 * This function is based on the following paper:
	 * https://mgasior.web.cern.ch/pap/biw2004.pdf
	 * https://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak/
	 * 	 * 
	 * In particular equation 10
	 */
	Point quadraticEstimation(const Point &a, const Point &b, const Point &c, unsigned maxFBin)
		{
		// Frequency estimation
		double maxBinEst = (double) maxFBin + (c.y - a.y) / (2 * (2 * b.y - a.y - c.y));
		double y_Fmax = startFreqency + maxBinEst * frequencyResolution; // convert bin to frequency

		// Amplitude estimation
		double f = (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y)) / ((a.x - b.x) * (a.x - c.x) * (c.x - b.x));
		double g = (pow(a.x, 2) * (b.y - c.y) + pow(b.x, 2) * (c.y - a.y) + pow(c.x, 2) * (a.y - b.y)) / ((a.x - b.x) * (a.x - c.x) * (b.x - c.x));
		double h = (pow(a.x, 2) * (b.x * c.y - c.x * b.y) + a.x * (pow(c.x, 2) * b.y - pow(b.x,2) * c.y)+ b.x * c.x * a.y * (b.x - c.x)) / ((a.x - b.x) * (a.x - c.x) * (b.x - c.x)); 
		double i = f * pow(y_Fmax,2) + g * y_Fmax + h;

		return { y_Fmax, i };
	}
};

/* Register hook */
static char n[] = "dft";
static char d[] = "This hook calculates the  dft on a window";
static HookPlugin<DftHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */
