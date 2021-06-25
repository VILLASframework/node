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

#include <villas/dumper.hpp>
#include <villas/hook.hpp>
#include <villas/path.h>
#include <villas/sample.h>
#include <villas/format.hpp>

#define DFT_MEM_DUMP

namespace villas {
namespace node {

class DftHook : public Hook {

protected:
	enum PaddingType {
		ZERO,
		SIG_REPEAT
	};

	enum WindowType {
		NONE,
		FLATTOP,
		HANN,
		HAMMING
	};

	std::shared_ptr<Dumper> origSigSync;
	std::shared_ptr<Dumper> windowdSigSync;
	std::shared_ptr<Dumper> phasorPhase;
	std::shared_ptr<Dumper> phasorAmplitude;
	std::shared_ptr<Dumper> phasorFreq;

	enum WindowType windowType;
	enum PaddingType paddingType;

	std::vector<std::vector<double>> smpMemory;
	std::vector<std::vector<std::complex<double>>> dftMatrix;
	std::vector<std::complex<double>> dftResults;
	std::vector<double> filterWindowCoefficents;
	std::vector<double> absDftResults;
	std::vector<double> absDftFreqs;


	uint64_t dftCalcCount;
	unsigned sampleRate;
	double startFreqency;
	double endFreqency;
	double frequencyResolution;
	struct timespec dftRate;
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
		smpMemory(),
		dftMatrix(),
		dftResults(),
		filterWindowCoefficents(),
		absDftResults(),
		absDftFreqs(),
		dftCalcCnt(0),
		sampleRate(0),
		startFreqency(0),
		endFreqency(0),
		frequencyResolution(0),
		dftRate({0, 0}),
		ppsIndex(0),
		windowSize(0),
		windowMultiplier(0),
		freqCount(0),
		windowCorretionFactor(0),
		lastDftCal({0, 0}),
		signalIndex()
	{
		bool debug = false;
		if (debug) {
			origSigSync = std::make_shared<Dumper>("/tmp/plot/origSigSync");
			windowdSigSync = std::make_shared<Dumper>("/tmp/plot/windowdSigSync");
			phasorPhase = std::make_shared<Dumper>("/tmp/plot/phasorPhase");
			phasorAmplitude = std::make_shared<Dumper>("/tmp/plot/phasorAmplitude");
			phasorFreq = std::make_shared<Dumper>("/tmp/plot/phasorFreq");
		}
	}

	virtual void prepare()
	{
		signal_list_clear(&signals);

		/* Initialize sample memory */
		smpMemory.clear();
		for (unsigned i = 0; i < signalIndex.size(); i++) {
			struct signal *freqSig;
			struct signal *amplSig;
			struct signal *phaseSig;
			struct signal *rocofSig;

			/* Add signals */
			freqSig = signal_create("amplitude", nullptr, SignalType::FLOAT);
			amplSig = signal_create("phase", nullptr, SignalType::FLOAT);
			phaseSig = signal_create("frequency", nullptr, SignalType::FLOAT);
			rocofSig = signal_create("rocof", nullptr, SignalType::FLOAT);

			if (!freqSig || !amplSig || !phaseSig || !rocofSig)
				throw RuntimeError("Failed to create new signals");

			vlist_push(&signals, freqSig);
			vlist_push(&signals, amplSig);
			vlist_push(&signals, phaseSig);
			vlist_push(&signals, rocofSig);

			smpMemory.emplace_back(windowSize, 0.0);
		}

		/* Calculate how much zero padding ist needed for a needed resolution */
		windowMultiplier = ceil(((double)sampleRate / windowSize) / frequencyResolution);

		freqCount = ceil((endFreqency - startFreqency) / frequencyResolution) + 1;

		logger->debug("FreqCount : {}", freqCount);

		/* Initialize matrix of dft coeffients */
		dftMatrix.clear();
		for (unsigned i = 0; i < freqCount; i++)
			dftMatrix.emplace_back(windowSize * windowMultiplier, 0.0);

		dftResults.resize(freqCount);
		filterWindowCoefficents.resize(windowSize);
		absDftResults.resize(freqCount);
		absDftFreqs.resize(freqCount);

		for (unsigned i = 0; i < absDftFreqs.size(); i++)
			absDftFreqs[i] = startFreqency + i * frequencyResolution;

		generateDftMatrix();
		calculateWindow(windowType);

		state = State::PREPARED;
	}

	virtual void parse(json_t *cfg)
	{
		const char *paddingTypeC = nullptr, *windowTypeC= nullptr;
		int ret;
		double dftRateIn = 0;
		json_error_t err;

		json_t *jsonChannelList = nullptr;

		assert(state != State::STARTED);

		Hook::parse(cfg);

		ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: F, s?: F, s?: F, s?: F , s?: i, s?: s, s?: s, s?: s, s?: b, s?: o}",
			"sample_rate", &sampleRate,
			"start_freqency", &startFreqency,
			"end_freqency", &endFreqency,
			"frequency_resolution", &frequencyResolution,
			"dft_rate", &dftRateIn,
			"window_size", &windowSize,
			"window_type", &windowTypeC,
			"padding_type", &paddingTypeC,
			"sync", &syncDft,
			"signal_index", &jsonChannelList
		);

		dftRate.tv_sec = (int) (1 / dftRateIn);
		dftRate.tv_nsec = fmod( 1 / dftRateIn, 1) * 1e9;

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

		smpMemPos++;

		bool runDft = false;
		if (syncDft) {
			//struct timespec timeDiff = time_diff(&lastDftCal, &smp->ts.origin);

			//double nextCalc = (lastDftCal.tv_sec + dftRate.tv_sec) * 1e9 + lastDftCal.tv_nsec + dftRate.tv_nsec;

			//double dftRateNSec = dftRate.tv_sec * 1e9 + dftRate.tv_nsec;
			//double smpNsec = smp->ts.origin.tv_sec * 1e9 + smp->ts.origin.tv_nsec;

			


			//if ( nextCalc < (smp->ts.origin.tv_sec * 1e9 + smp->ts.origin.tv_nsec) && fmod( smpNsec, dftRateNSec ) < (110*1e9/(sampleRate)))
			//	runDft = true;

			if (smp->ts.origin.tv_sec != lastDftCal.tv_sec)
				runDft = true;

			//if ((fmod(smp->ts.origin.tv_sec*1e9 + smp->ts.origin.tv_nsec, 1e9/dftRate) < (1e9/(dftRate+1))) && ((timeDiff.tv_sec*1e9+timeDiff.tv_nsec) > (1e9/dftRate)))
			//if ((timeDiff.tv_sec*1e9+timeDiff.tv_nsec) > (1e9/dftRate) && )
			//	runDft = true;
		}

		if (runDft) {
			lastDftCal = smp->ts.origin;

			// Debugging for pps signal this should only be temporary
			//if (ppsSigSync) {
				//double tmpPPSWindow[windowSize];

				//for (unsigned i = 0; i< windowSize; i++)
				//	tmpPPSWindow[i] = ppsMemory[(i + smpMemPos) % windowSize];

				//ppsSigSync->writeDataBinary(windowSize, tmpPPSWindow);	
			//}
		
			#pragma omp parallel for
			for (unsigned i = 0; i < signalIndex.size(); i++) {

				calculateDft(PaddingType::ZERO, smpMemory[i], dftResults[i], smpMemPos);




				double maxF = 0;
				double maxA = 0;
				unsigned maxPos = 0;
				double tmpImag[freqCount], tmpReal[freqCount], absVal[freqCount];

				
				for (unsigned j = 0; j < freqCount; j++) {
					int multiplier = paddingType == PaddingType::ZERO
						? 1
						: windowMultiplier;
					absDftResults[i][j] = abs(dftResults[i][j]) * 2 / (windowSize * windowCorretionFactor * multiplier);
					absVal[j] = absDftResults[i][j];
					if (maxA < absDftResults[i][j]) {
						maxF = absDftFreqs[j];
						maxA = absDftResults[i][j];
						maxPos = j;
					}
					tmpImag[j] = dftResults[i][maxPos].imag();
					tmpReal[j] = dftResults[i][maxPos].real();
				}
				windowdSigSync->writeDataBinary(freqCount, tmpImag);
				origSigSync -> writeDataBinary(freqCount, absVal);
				ppsSigSync->writeDataBinary(freqCount, tmpReal);


				if (dftCalcCnt > 1) {
					if (phasorFreq)
						phasorFreq->writeData(1, &maxF);

					smp->data[i * 4].f = maxF; /* Frequency */
					smp->data[i * 4 + 1].f = (maxA / pow(2, 0.5)); /* Amplitude */
					smp->data[i * 4 + 2].f = atan2(dftResults[maxPos].imag(), dftResults[maxPos].real()); /* Phase */
					smp->data[i * 4 + 3].f = 0; /* RoCof */

					if (phasorPhase)
						phasorPhase->writeData(1, &(smp->data[i * 4 + 2].f));
				}
			}

			//the following is a debug output and currently only for channel 0
			if (windowSize < smpMemPos){ 
				if (phasorFreq)
					phasorFreq->writeDataBinary(1, &(smp->data[0 * 4 + 0].f));

				if (phasorPhase)
					phasorPhase->writeDataBinary(1, &(smp->data[0 * 4 + 2].f));

				if (phasorAmplitude)
					phasorAmplitude->writeDataBinary(1, &(smp->data[0 * 4 + 1].f));				
			}

			smp->length = windowSize < smpMemPos ? signalIndex.size() * 4 : 0;

			dftCalcCount++;
		}

		if ((smp->sequence - lastSequence) > 1)
			logger->warn("Calculation is not Realtime. {} sampled missed", smp->sequence - lastSequence);

		lastSequence = smp->sequence;

		if (runDft)
			return Reason::OK;

		return Reason::SKIP_SAMPLE;
	}

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

	void calculateDft(enum PaddingType padding, std::vector<double> &ringBuffer, unsigned ringBufferPos)
	{
		/* RingBuffer size needs to be equal to windowSize
		 * prepare sample window The following parts can be combined */
		double tmpSmpWindow[windowSize];

		for (unsigned i = 0; i< windowSize; i++)
			tmpSmpWindow[i] = ringBuffer[(i + ringBufferPos) % windowSize];

#ifdef DFT_MEM_DUMP

		//if (origSigSync)
		// 	origSigSync->writeDataBinary(windowSize, tmpSmpWindow);
#endif

		for (unsigned i = 0; i < windowSize; i++)
			tmpSmpWindow[i] *= filterWindowCoefficents[i];

#ifdef DFT_MEM_DUMP

		//if (windowdSigSync)
		// 	windowdSigSync->writeDataBinary(windowSize, tmpSmpWindow);

#endif

		for (unsigned i = 0; i < freqCount; i++) {
			dftResults[i] = 0;
			for (unsigned j = 0; j < windowSize * windowMultiplier; j++) {
				if (padding == PaddingType::ZERO) {
					if (j < (windowSize))
						dftResults[i] += tmpSmpWindow[j] * dftMatrix[i][j];
					else
						dftResults[i] += 0;
				}
				else if (padding == PaddingType::SIG_REPEAT) /* Repeat samples */
					dftResults[i] += tmpSmpWindow[j % windowSize] * dftMatrix[i][j];
			}
		}
	}

	void calculateWindow(enum WindowType windowTypeIn)
	{
		switch (windowTypeIn) {
			case WindowType::FLATTOP:
				for (unsigned i = 0; i < windowSize; i++) {
					filterWindowCoefficents[i] = 	0.21557895
									- 0.41663158 * cos(2 * M_PI * i / (windowSize))
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
};

/* Register hook */
static char n[] = "dft";
static char d[] = "This hook calculates the  dft on a window";
static HookPlugin<DftHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */
