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
#include <villas/format.hpp>

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

	std::shared_ptr<Dumper> origSigSync;
	std::shared_ptr<Dumper> windowdSigSync;
	std::shared_ptr<Dumper> phasorPhase;
	std::shared_ptr<Dumper> phasorAmplitude;
	std::shared_ptr<Dumper> phasorFreq;
	std::shared_ptr<Dumper> ppsSigSync;

	enum WindowType windowType;
	enum PaddingType paddingType;
	enum FreqEstimationType freqEstType;

	std::vector<std::vector<double>> smpMemory;
	std::vector<double> ppsMemory;//this is just temporary for debugging
	std::vector<std::vector<std::complex<double>>> dftMatrix;
	std::vector<std::vector<std::complex<double>>> dftResults;
	std::vector<double> filterWindowCoefficents;
	std::vector<std::vector<double>> absDftResults;
	std::vector<double> absDftFreqs;

	uint64_t dftCalcCount;
	unsigned sampleRate;
	double startFreqency;
	double endFreqency;
	double frequencyResolution;
	unsigned dftRate;
	unsigned ppsIndex;
	unsigned windowSize;
	unsigned windowMultiplier;	/**< Multiplyer for the window to achieve frequency resolution */
	unsigned freqCount;		/**< Number of requency bins that are calculated */
	bool syncDft;

	uint64_t smpMemPos;
	uint64_t lastSequence;

	std::complex<double> omega;

	double windowCorretionFactor;
	struct timespec lastDftCal;

	std::vector<int> signalIndex;	/**< A list of signalIndex to do dft on */

public:
	DftHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		windowType(WindowType::NONE),
		paddingType(PaddingType::ZERO),
		freqEstType(FreqEstimationType::NONE),
		smpMemory(),
		ppsMemory(),
		dftMatrix(),
		dftResults(),
		filterWindowCoefficents(),
		absDftResults(),
		absDftFreqs(),
		dftCalcCount(0),
		sampleRate(0),
		startFreqency(0),
		endFreqency(0),
		frequencyResolution(0),
		dftRate(0),
		ppsIndex(0),
		windowSize(0),
		windowMultiplier(0),
		freqCount(0),
		syncDft(0),
		smpMemPos(0),
		lastSequence(0),
		windowCorretionFactor(0),
		lastDftCal({0, 0}),
		signalIndex()
	{
		logger = logging.get("hook:dft");

		format = format_type_lookup("villas.human");

		if (logger->level() <= SPDLOG_LEVEL_DEBUG) {
#ifdef DFT_MEM_DUMP
			origSigSync = std::make_shared<Dumper>("/tmp/plot/origSigSync");
			windowdSigSync = std::make_shared<Dumper>("/tmp/plot/windowdSigSync");
			ppsSigSync = std::make_shared<Dumper>("/tmp/plot/ppsSigSync");
#endif
			phasorPhase = std::make_shared<Dumper>("/tmp/plot/phasorPhase");
			phasorAmplitude = std::make_shared<Dumper>("/tmp/plot/phasorAmplitude");
			phasorFreq = std::make_shared<Dumper>("/tmp/plot/phasorFreq");
			
		}
	}

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

		/* Initialize temporary ppsMemory */
		ppsMemory.clear();
		ppsMemory.resize(windowSize, 0.0);

		/* Calculate how much zero padding ist needed for a needed resolution */
		windowMultiplier = ceil(((double) sampleRate / windowSize) / frequencyResolution);

		freqCount = ceil((endFreqency - startFreqency) / frequencyResolution) + 1;

		/* Initialize matrix of dft coeffients */
		dftMatrix.clear();
		for (unsigned i = 0; i < freqCount; i++)
			dftMatrix.emplace_back(windowSize * windowMultiplier, 0.0);

		/* Initalize dft results matrix */
		dftResults.clear();
		for (unsigned i = 0; i < signalIndex.size(); i++){
			dftResults.emplace_back(freqCount, 0.0);
			absDftResults.emplace_back(freqCount, 0.0);
		}

		filterWindowCoefficents.resize(windowSize);

		for (unsigned i = 0; i < freqCount; i++) {
			absDftFreqs.emplace_back(startFreqency + i * frequencyResolution);
		}

		generateDftMatrix();
		calculateWindow(windowType);

		state = State::PREPARED;
	}

	virtual void parse(json_t *cfg)
	{
		const char *paddingTypeC = nullptr, *windowTypeC = nullptr, *freqEstimateTypeC = nullptr;
		int ret;
		json_error_t err;

		json_t *jsonChannelList = nullptr;

		assert(state != State::STARTED);

		Hook::parse(cfg);

		ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: F, s?: F, s?: F, s?: i , s?: i, s?: s, s?: s, s?: s, s?: b, s?: o}",
			"sample_rate", &sampleRate,
			"start_freqency", &startFreqency,
			"end_freqency", &endFreqency,
			"frequency_resolution", &frequencyResolution,
			"dft_rate", &dftRate,
			"window_size", &windowSize,
			"window_type", &windowTypeC,
			"padding_type", &paddingTypeC,
			"freq_estimate_type", &freqEstimateTypeC,
			"sync", &syncDft,
			"signal_index", &jsonChannelList,
			"pps_index", &ppsIndex
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-dft");

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
				logger->warn("Could not parse channel list. Please check documentation for syntax");
		}
		else
			throw ConfigError(jsonChannelList, "node-config-node-signal", "No parameter signalIndex given.");

		if (!windowTypeC) {
			logger->info("No Window type given, assume no windowing");
			windowType = WindowType::NONE;
		}
		else if (strcmp(windowTypeC, "flattop") == 0)
			windowType = WindowType::FLATTOP;
		else if (strcmp(windowTypeC, "hamming") == 0)
			windowType = WindowType::HAMMING;
		else if (strcmp(windowTypeC, "hann") == 0)
			windowType = WindowType::HANN;
		else {
			logger->info("Window type {} not recognized, assume no windowing", windowTypeC);
			windowType = WindowType::NONE;
		}

		if (!paddingTypeC) {
			logger->info("No Padding type given, assume no zeropadding");
			paddingType = PaddingType::ZERO;
		}
		else if (strcmp(paddingTypeC, "signal_repeat") == 0)
			paddingType = PaddingType::SIG_REPEAT;
		else {
			logger->info("Padding type {} not recognized, assume zero padding", paddingTypeC);
			paddingType = PaddingType::ZERO;
		}

		if (!freqEstimateTypeC) {
			logger->info("No Frequency estimation type given, assume no none");
			freqEstType = FreqEstimationType::NONE;
		}
		else if (strcmp(freqEstimateTypeC, "quadratic") == 0)
			freqEstType = FreqEstimationType::QUADRATIC;

		if (endFreqency < 0 || endFreqency > sampleRate)
			throw ConfigError(cfg, err, "node-config-hook-dft", "End frequency must be smaller than sampleRate {}", sampleRate);

		if (frequencyResolution > ((double)sampleRate/windowSize))
			throw ConfigError(cfg, err, "node-config-hook-dft", "The maximum frequency resolution with smaple_rate:{} and window_site:{} is {}", sampleRate, windowSize, ((double)sampleRate/windowSize));

		state = State::PARSED;
	}

	virtual Hook::Reason process(struct sample *smp)
	{
		assert(state == State::STARTED);

		for (unsigned i = 0; i < signalIndex.size(); i++)
			smpMemory[i][smpMemPos % windowSize] = smp->data[signalIndex[i]].f;

		// Debugging for pps signal this should only be temporary
		if (ppsSigSync)
			ppsMemory[smpMemPos % windowSize] = smp->data[ppsIndex].f;

		smpMemPos++;

		bool runDft = false;
		if (syncDft) {
			struct timespec timeDiff = time_diff(&lastDftCal, &smp->ts.origin);
			if ((timeDiff.tv_sec*1e9+timeDiff.tv_nsec) >  (1e9/dftRate))
				runDft = true;

			//if (lastDftCal.tv_sec != smp->ts.origin.tv_sec)
			//	runDft = true;
		}

		if (runDft) {
			lastDftCal = smp->ts.origin;

			// Debugging for pps signal this should only be temporary
				if (ppsSigSync) {
					double tmpPPSWindow[windowSize];

					for (unsigned i = 0; i< windowSize; i++)
						tmpPPSWindow[i] = ppsMemory[(i + smpMemPos) % windowSize];

					ppsSigSync->writeData(windowSize, tmpPPSWindow);	
				}

			#pragma omp parallel for
			for (unsigned i = 0; i < signalIndex.size(); i++) {

				calculateDft(PaddingType::ZERO, smpMemory[i], dftResults[i], smpMemPos);

				double maxF = 0;
				double maxA = 0;
				unsigned maxPos = 0;

				for (unsigned j = 0; j < freqCount; j++) {
					int multiplier = paddingType == PaddingType::ZERO
						? 1
						: windowMultiplier;
					absDftResults[i][j] = abs(dftResults[i][j]) * 2 / (windowSize * windowCorretionFactor * multiplier);
					if (maxA < absDftResults[i][j]) {
						maxF = absDftFreqs[j];
						maxA = absDftResults[i][j];
						maxPos = j;
					}
				}

				if (freqEstType == FreqEstimationType::QUADRATIC) {
					if (maxPos < 1 || maxPos >= freqCount - 1)
						logger->warn("Maximum frequency bin lies on window boundary. Using non-estimated results!");
					else {
						Point a = { absDftFreqs[maxPos - 1], absDftResults[i][maxPos - 1] };
						Point b = { absDftFreqs[maxPos + 0], absDftResults[i][maxPos + 0] };
						Point c = { absDftFreqs[maxPos + 1], absDftResults[i][maxPos + 1] };
						
						Point estimate = quadraticEstimation(a, b, c, maxPos);

					maxF = estimate.x;
					maxA = estimate.y;
				}
				}

<<<<<<< HEAD
				if (dftCalcCnt > 1) {
					if (phasorFreq)
						phasorFreq->writeDataBinary(1, &maxF);
=======
				if (windowSize < smpMemPos) {
>>>>>>> dft: make sure the dft mem is fully initalized

					smp->data[i * 4 + 0].f = maxF; /* Frequency */
					smp->data[i * 4 + 1].f = (maxA / pow(2, 0.5)); /* Amplitude */
					smp->data[i * 4 + 2].f = atan2(dftResults[i][maxPos].imag(), dftResults[i][maxPos].real()); /* Phase */
					smp->data[i * 4 + 3].f = 0; /* RoCof */

				}
			}

			//the following is a debug output and currently only for channel 0
			if (windowSize < smpMemPos){ 
				if (phasorFreq)
					phasorFreq->writeData(1, &(smp->data[0 * 4 + 0].f));

					if (phasorPhase)
					phasorPhase->writeData(1, &(smp->data[0 * 4 + 2].f));

					if (phasorAmplitude)
					phasorAmplitude->writeData(1, &(smp->data[0 * 4 + 1].f));				
			}

			smp->length = windowSize < smpMemPos ? signalIndex.size() * 4 : 0;

			dftCalcCount++;
		}

		if (smp->sequence - lastSequence > 1)
			logger->warn("Calculation is not Realtime. {} sampled missed", smp->sequence - lastSequence);

		lastSequence = smp->sequence;

		if(runDft && windowSize < smpMemPos)
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
		unsigned startBin = floor(startFreqency / frequencyResolution);

		for (unsigned i = 0; i <  freqCount ; i++) {
			for (unsigned j = 0 ; j < windowSize * windowMultiplier ; j++)
				dftMatrix[i][j] = pow(omega, (i + startBin) * j);
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

		if (origSigSync)
			origSigSync->writeDataBinary(windowSize, tmpSmpWindow);

		if (dftCalcCount > 1 && phasorAmplitude)
			phasorAmplitude->writeData(1, &tmpSmpWindow[windowSize - 1]);

#endif

		for (unsigned i = 0; i < windowSize; i++)
			tmpSmpWindow[i] *= filterWindowCoefficents[i];

#ifdef DFT_MEM_DUMP

		if (windowdSigSync)
			windowdSigSync->writeDataBinary(windowSize, tmpSmpWindow);

#endif

		for (unsigned i = 0; i < freqCount; i++) {
			results[i] = 0;

			for (unsigned j = 0; j < windowSize * windowMultiplier; j++) {
				if (padding == PaddingType::ZERO) {
					if (j < (windowSize))
						results[i] += tmpSmpWindow[j] * dftMatrix[i][j];
					else
						results[i] += 0;
				}
				else if (padding == PaddingType::SIG_REPEAT) /* Repeat samples */
					results[i] += tmpSmpWindow[j % windowSize] * dftMatrix[i][j];
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
					windowCorretionFactor += filterWindowCoefficents[i];
				}
				break;

			case WindowType::HAMMING:
			case WindowType::HANN: {
				double a0 = 0.5; /* This is the hann window */
				if (windowTypeIn == WindowType::HAMMING)
					a0 = 25./46;

				for (unsigned i = 0; i < windowSize; i++) {
					filterWindowCoefficents[i] = a0 - (1 - a0) * cos(2 * M_PI * i / (windowSize));
					windowCorretionFactor += filterWindowCoefficents[i];
				}

				break;
			}

			default:
				for (unsigned i = 0; i < windowSize; i++) {
					filterWindowCoefficents[i] = 1;
					windowCorretionFactor += filterWindowCoefficents[i];
				}
				break;
		}

		windowCorretionFactor /= windowSize;
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
