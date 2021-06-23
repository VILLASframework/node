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
	std::vector<std::complex<double>> dftResults;
	std::vector<double> filterWindowCoefficents;
	std::vector<double> absDftResults;
	std::vector<double> absDftFreqs;

	uint64_t dftCalcCnt;
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
		dftCalcCnt(0),
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
			origSigSync = std::make_shared<Dumper>("/tmp/plot/origSigSync");
			windowdSigSync = std::make_shared<Dumper>("/tmp/plot/windowdSigSync");
			phasorPhase = std::make_shared<Dumper>("/tmp/plot/phasorPhase");
			phasorAmplitude = std::make_shared<Dumper>("/tmp/plot/phasorAmplitude");
			phasorFreq = std::make_shared<Dumper>("/tmp/plot/phasorFreq");
			ppsSigSync = std::make_shared<Dumper>("/tmp/plot/ppsSigSync");
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

			smpMemory.emplace_back(std::vector<double>(windowSize, 0.0));
		}

		//temporary ppsMemory
		ppsMemory.clear();
		ppsMemory.resize(windowSize, 0.0);

		/* Calculate how much zero padding ist needed for a needed resolution */
		windowMultiplier = ceil(((double)sampleRate / windowSize) / frequencyResolution);

		freqCount = ceil((endFreqency - startFreqency) / frequencyResolution) + 1;

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

		//debugging for pps signal this should only be temporary
		if (ppsSigSync)
			ppsMemory[smpMemPos % windowSize] = smp->data[ppsIndex].f;
		//debugging for pps signal this should only be temporary

		smpMemPos++;

		bool runDft = false;
		if (syncDft) {
			struct timespec timeDiff = time_diff(&lastDftCal, &smp->ts.origin);
			if (timeDiff.tv_sec > 0)
				runDft = true;

			//if (lastDftCal.tv_sec != smp->ts.origin.tv_sec)
			//	runDft = true;
		}

		if (runDft) {
			lastDftCal = smp->ts.origin;
			for (unsigned i = 0; i < signalIndex.size(); i++) {

				//debugging for pps signal this should only be temporary
				if (ppsSigSync) {
					double tmpPPSWindow[windowSize];
					for (unsigned i = 0; i< windowSize; i++)
						tmpPPSWindow[i] = ppsMemory[(i + smpMemPos) % windowSize];
					ppsSigSync->writeData(windowSize, tmpPPSWindow);	
				}
				//debugging for pps signal this should only be temporary

				calculateDft(PaddingType::ZERO, smpMemory[i], smpMemPos);
				double maxF = 0;
				double maxA = 0;
				int maxPos = 0;

				for (unsigned i = 0; i<freqCount; i++) {
					absDftResults[i] = abs(dftResults[i]) * 2 / (windowSize * windowCorretionFactor * ((paddingType == PaddingType::ZERO)?1:windowMultiplier));
					if (maxA < absDftResults[i]) {
						maxF = absDftFreqs[i];
						maxA = absDftResults[i];
						maxPos = i;
					}
				}

				if (freqEstType == FreqEstimationType::QUADRATIC) {
					Point estimate = quadraticEstimation(absDftFreqs[maxPos - 1], absDftFreqs[maxPos], absDftFreqs[maxPos + 1], absDftResults[maxPos - 1] , absDftResults[maxPos] , absDftResults[maxPos + 1], maxPos);
					logger->info("1: {};{}  2: {};{} 3: {};{} estimate: {}:{} ", absDftFreqs[maxPos - 1], absDftResults[maxPos - 1], absDftFreqs[maxPos], absDftResults[maxPos], absDftFreqs[maxPos + 1], absDftResults[maxPos + 1], estimate.x, estimate.y);
					maxF = estimate.x;
					maxA = estimate.y;
				}

				if (dftCalcCnt > 1) {
					if (phasorFreq)
						phasorFreq->writeDataBinary(1, &maxF);

					smp->data[i * 4].f = maxF; /* Frequency */
					smp->data[i * 4 + 1].f = (maxA / pow(2, 0.5)); /* Amplitude */
					smp->data[i * 4 + 2].f = atan2(dftResults[maxPos].imag(), dftResults[maxPos].real()); /* Phase */
					smp->data[i * 4 + 3].f = 0; /* RoCof */

					if (phasorPhase)
						phasorPhase->writeData(1, &(smp->data[i * 4 + 2].f));
					if (phasorAmplitude)
						phasorAmplitude->writeData(1, &(smp->data[i * 4 + 1].f));
				}
			}
			dftCalcCnt++;
			smp->length = signalIndex.size() * 4;
		}

		if ((smp->sequence - lastSequence) > 1)
			logger->warn("Calculation is not Realtime. {} sampled missed", smp->sequence - lastSequence);

		lastSequence = smp->sequence;

		if (runDft)
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
	void calculateDft(enum PaddingType padding, std::vector<double> &ringBuffer, unsigned ringBufferPos)
	{
		/* RingBuffer size needs to be equal to windowSize
		 * prepare sample window The following parts can be combined */
		double tmpSmpWindow[windowSize];

		for (unsigned i = 0; i< windowSize; i++)
			tmpSmpWindow[i] = ringBuffer[(i + ringBufferPos) % windowSize];

		if (origSigSync)
			origSigSync->writeDataBinary(windowSize, tmpSmpWindow);

		//if (dftCalcCnt > 1 && phasorAmplitude)
		//	phasorAmplitude->writeData(1, &tmpSmpWindow[windowSize - 1]);

		for (unsigned i = 0; i< windowSize; i++)
			tmpSmpWindow[i] *= filterWindowCoefficents[i];

		if (windowdSigSync)
			windowdSigSync->writeDataBinary(windowSize, tmpSmpWindow);

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
	 * 
	 */
	Point quadraticEstimation(double x1, double x2, double x3, double y1, double y2, double y3, unsigned maxFBin)
	{

		//double y_max = x2 - (y3 - y1) / (2 * ( 2 * y2  - y1 - y3));
		double y_Fmax = 0;

		/*Quadratic Method */
		double maxBinEst =  ((y3 - y1) / ( 2 * ( 2 * y2 - y1 - y3)));
		


		logger->info("y3 - y1 {} ; y_max : {}", (y3 - y1), maxBinEst);

		maxBinEst = (double)maxFBin + maxBinEst;

		/*Jain’s Method
		if (y1 > y3)
		{
			y_max = (double)maxFBin - 1 + (((y2 / y1) / ( 1 + (y2/y1))));
		}
		else
		{
			y_max = (double)maxFBin + ((y3 / y2) / ( 1 + (y3 / y2)));
		}*/
		
		y_Fmax = startFreqency + maxBinEst * frequencyResolution;


		//calc amplitude
		//double h = (y2 - ( (y1 * x2^2) / x1^2)) / (1 - x2^2 / x1^2 )

		//double h = (y2 - ( (y1 * pow(x2, 2)) / pow(x1, 2))) / (1 - pow(x2, 2) / pow(x1, 2));

		//double h = ( - y3 + ( (y1 * pow(x3, 2)) / pow(x1, 2))) / (pow(x3, 2) / pow(x1, 2) - 1 );

		double a = (x1 * (y2-y3)+x2 * (y3-y1)+x3 * (y1-y2))/((x1-x2) * (x1-x3) * (x3-x2));
		double b = (pow(x1,2)*(y2-y3)+pow(x2,2)*(y3-y1)+pow(x3,2) * (y1-y2))/((x1-x2) * (x1-x3) * (x2-x3));
		double c = (pow(x1,2) * (x2 * y3 - x3 *y2)+x1 * (pow(x3,2) * y2 - pow(x2,2) * y3)+ x2 * x3 * y1 * (x2-x3))/((x1-x2) * (x1-x3) * (x2-x3));

		 
		double h = a * pow(y_Fmax,2) + b * y_Fmax + c;

		Point ret = {y_Fmax, h};

		return ret;
	}
};

/* Register hook */
static char n[] = "dft";
static char d[] = "This hook calculates the  dft on a window";
static HookPlugin<DftHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */
