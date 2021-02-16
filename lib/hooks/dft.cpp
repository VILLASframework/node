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
#include <villas/dumper.hpp>
#include <villas/hook.hpp>
#include <villas/path.h>
#include <villas/sample.h>
#include <villas/io.h>
#include <villas/plugin.h>
#include <complex>

namespace villas {
namespace node {

class DftHook : public Hook {

protected:
	enum paddingTypeEnum {
		ZERO,
		SIG_REPEAT
	};

	enum windowTypeEnum {
		NONE,
		FLATTOP,
		HANN,
		HAMMING
	};

	Dumper* origSigSync;
	Dumper* ppsSigSync;
	Dumper* windowdSigSync;
	Dumper* phasorPhase;
	Dumper* phasorAmplitude;
	Dumper* phasorFreq;

	windowTypeEnum windowType;
	paddingTypeEnum paddingType;

	struct format_type *format;

	double** smpMemory;
	std::complex<double>** dftMatrix;
	std::complex<double>* dftResults;
	double* filterWindowCoefficents;
	double* absDftResults;
	double* absDftFreqs;

	uint64_t dftCalcCnt;
	uint sampleRate;
	double startFreqency;
	double endFreqency;
	double frequencyResolution;
	uint dftRate;
	uint windowSize;
	uint windowMultiplier;//multiplyer for the window to achieve frequency resolution
	uint freqCount;//number of requency bins that are calculated
	bool syncDft;

	uint64_t smpMemPos;
	uint64_t lastSequence;

	std::complex<double> omega;
	std::complex<double> M_I;

	double windowCorretionFactor;
	timespec lastDftCal;

	int* signalIndex;//a list of signalIndex to do dft on
	uint signalCnt;//number of signalIndex given by config file

public:
	DftHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		windowType(windowTypeEnum::NONE),
		paddingType(paddingTypeEnum::ZERO),
		dftCalcCnt(0),
		sampleRate(0),
		startFreqency(0),
		endFreqency(0),
		frequencyResolution(0),
		dftRate(0),
		windowSize(0),
		windowMultiplier(0),
		freqCount(0),
		syncDft(0),
		smpMemPos(0),
		lastSequence(0),
		M_I(0.0,1.0),
		windowCorretionFactor(0),
		lastDftCal({0,0}),
		signalCnt(0)
	{
		format = format_type_lookup("villas.human");

		origSigSync = new Dumper("/tmp/plot/origSigSync");
		ppsSigSync = new Dumper("/tmp/plot/ppsSigSync");
		windowdSigSync = new Dumper("/tmp/plot/windowdSigSync");
		phasorPhase = new Dumper("/tmp/plot/phasorPhase");
		phasorAmplitude = new Dumper("/tmp/plot/phasorAmplitude");
		phasorFreq = new Dumper("/tmp/plot/phasorFreq");
	}

	virtual ~DftHook()
	{
		delete smpMemory;
		delete origSigSync;
		delete ppsSigSync;
		delete windowdSigSync;
		delete phasorPhase;
		delete phasorAmplitude;
		delete phasorFreq;
	}

	virtual void prepare()
	{

		signal_list_clear(&signals);
		/* init sample memory */
		smpMemory = new double*[signalCnt];
		if (!smpMemory)
			throw MemoryAllocationError();

		for (uint i = 0; i < signalCnt; i++) {
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

			smpMemory[i] = new double[windowSize];
			if (!smpMemory[i])
				throw MemoryAllocationError();

			for (uint j = 0; j < windowSize; j++)
				smpMemory[i][j] = 0;
		}

		windowMultiplier = ceil(((double)sampleRate / windowSize) / frequencyResolution); //calculate how much zero padding ist needed for a needed resolution

		freqCount = ceil((endFreqency - startFreqency) / frequencyResolution) + 1;

		/* init matrix of dft coeffients */
		dftMatrix = new std::complex<double>*[freqCount];
		if (!dftMatrix)
			throw MemoryAllocationError();

		for (uint i = 0; i < freqCount; i++) {
			dftMatrix[i] = new std::complex<double>[windowSize * windowMultiplier]();
			if (!dftMatrix[i])
				throw MemoryAllocationError();
		}

		dftResults = new std::complex<double>[freqCount]();
		if (!dftResults)
			throw MemoryAllocationError();

		filterWindowCoefficents = new double[windowSize];
		if (!filterWindowCoefficents)
			throw MemoryAllocationError();

		absDftResults = new double[freqCount];
		if (!absDftResults)
			throw MemoryAllocationError();

		absDftFreqs = new double[freqCount];
		if (!absDftFreqs)
			throw MemoryAllocationError();

		for (uint i = 0; i < freqCount; i++)
			absDftFreqs[i] = startFreqency + i * frequencyResolution;

		generateDftMatrix();
		calcWindow(windowType);

		state = State::PREPARED;
	}

	virtual void parse(json_t *cfg)
	{
		const char *paddingTypeC = nullptr, *windowTypeC= nullptr;
		int ret;
		json_error_t err;

		json_t *jsonChannelList = nullptr;

		assert(state != State::STARTED);

		Hook::parse(cfg);

		state = State::PARSED;

		ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: F, s?: F, s?: F, s?: i , s?: i, s?: s, s?: s, s?: b, s?: o}",
			"sampleRate", &sampleRate,
			"startFreqency", &startFreqency,
			"endFreqency", &endFreqency,
			"frequencyResolution", &frequencyResolution,
			"dftRate", &dftRate,
			"windowSize", &windowSize,
			"windowType", &windowTypeC,
			"paddingType", &paddingTypeC,
			"sync", &syncDft,
			"signalIndex", &jsonChannelList
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-dft");

		if (jsonChannelList != nullptr) {
			if (jsonChannelList->type == JSON_ARRAY) {
				signalCnt = json_array_size(jsonChannelList);
				signalIndex = new int[signalCnt];
				if (!signalIndex)
					throw MemoryAllocationError();

				size_t i;
				json_t *jsonValue;
				json_array_foreach(jsonChannelList, i, jsonValue) {
					if (!json_is_number(jsonValue))
						throw ConfigError(jsonValue, "node-config-hook-dft-channel", "Values must be given as array of integer values!");
					signalIndex[i] = json_number_value(jsonValue);
				}
			}
			else if (jsonChannelList->type == JSON_INTEGER) {
				signalCnt = 1;
				signalIndex = new int[signalCnt];
				if (!signalIndex)
					throw MemoryAllocationError();
				if (!json_is_number(jsonChannelList))
					throw ConfigError(jsonChannelList, "node-config-hook-dft-channel", "Value must be given as integer value!");
				signalIndex[0] = json_number_value(jsonChannelList);
			}
			else
				warning("Could not parse channel list. Please check documentation for syntax");
		}
		else
			throw ConfigError(jsonChannelList, "node-config-node-signal", "No parameter channel given.");

		if (!windowTypeC) {
			info("No Window type given, assume no windowing");
			windowType = windowTypeEnum::NONE;
		}
		else if (strcmp(windowTypeC, "flattop") == 0)
			windowType = windowTypeEnum::FLATTOP;
		else if (strcmp(windowTypeC, "hamming") == 0)
			windowType = windowTypeEnum::HAMMING;
		else if (strcmp(windowTypeC, "hann") == 0)
			windowType = windowTypeEnum::HANN;
		else {
			info("Window type %s not recognized, assume no windowing",windowTypeC);
			windowType = windowTypeEnum::NONE;
		}

		if (!paddingTypeC) {
			info("No Padding type given, assume no zeropadding");
			paddingType = paddingTypeEnum::ZERO;
		}
		else if (strcmp(paddingTypeC, "signal_repeat") == 0)
			paddingType = paddingTypeEnum::SIG_REPEAT;
		else {
			info("Padding type %s not recognized, assume zero padding",paddingTypeC);
			paddingType = paddingTypeEnum::ZERO;
		}

		if (endFreqency < 0 || endFreqency > sampleRate)
			throw ConfigError(cfg, err, "node-config-hook-dft","End frequency must be smaller than sampleRate {}",sampleRate);

		if (frequencyResolution > ((double)sampleRate/windowSize))
			throw ConfigError(cfg, err, "node-config-hook-dft","The maximum frequency resolution with smaple_rate:{} and window_site:{} is {}",sampleRate, windowSize, ((double)sampleRate/windowSize));



	}

	virtual Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);
		for (uint i = 0; i< signalCnt; i++)
			smpMemory[i][smpMemPos % windowSize] = smp->data[signalIndex[i]].f;

		smpMemPos++;

		bool runDft = false;
		if (syncDft) {
			if (lastDftCal.tv_sec != smp->ts.origin.tv_sec)
				runDft = true;
		}
		lastDftCal = smp->ts.origin;

		if (runDft) {
			for (uint i = 0; i < signalCnt; i++) {
				calcDft(paddingTypeEnum::ZERO, smpMemory[i], smpMemPos);
				double maxF = 0;
				double maxA = 0;
				int maxPos = 0;

				for (uint i = 0; i<freqCount; i++) {
					absDftResults[i] = abs(dftResults[i]) * 2 / (windowSize * windowCorretionFactor * ((paddingType == paddingTypeEnum::ZERO)?1:windowMultiplier));
					if (maxA < absDftResults[i]) {
						maxF = absDftFreqs[i];
						maxA = absDftResults[i];
						maxPos = i;
					}
				}

				if (dftCalcCnt > 1) {
					phasorFreq->writeData(1,&maxF);

					smp->data[i * 4].f = maxF;//frequency
					smp->data[i * 4 + 1].f = (maxA / pow(2,1./2));//amplitude
					smp->data[i * 4 + 2].f = atan2(dftResults[maxPos].imag(), dftResults[maxPos].real());//phase
					smp->data[i * 4 + 3].f = 0;//rocof

					phasorPhase->writeData(1,&(smp->data[i * 4 + 2].f));
				}
			}
			dftCalcCnt++;
			smp->length = signalCnt * 4;
		}

		if ((smp->sequence - lastSequence) > 1)
			warning("Calculation is not Realtime. %li sampled missed",smp->sequence - lastSequence);

		lastSequence = smp->sequence;

		if (runDft)
			return Reason::OK;

		return Reason::SKIP_SAMPLE;
	}

	void generateDftMatrix() {
		using namespace std::complex_literals;

		omega = exp((-2 * M_PI * M_I) / (double)(windowSize * windowMultiplier));
		uint startBin = floor(startFreqency / frequencyResolution);

		for (uint i = 0; i <  freqCount ; i++) {
			for (uint j=0 ; j < windowSize * windowMultiplier ; j++) {
				dftMatrix[i][j] = pow(omega, (i + startBin) * j);
			}
		}
	}


	void calcDft(paddingTypeEnum padding, double *ringBuffer, uint ringBufferPos) {
		/* ringBuffer size needs to be equal to windowSize */
		/* prepare sample window The following parts can be combined */
		double tmpSmpWindow[windowSize];

		for (uint i = 0; i< windowSize; i++)
			tmpSmpWindow[i] = ringBuffer[(i + ringBufferPos) % windowSize];

		origSigSync->writeData(windowSize,tmpSmpWindow);

		if (dftCalcCnt > 1)
			phasorAmplitude->writeData(1,&tmpSmpWindow[windowSize - 1]);

		for (uint i = 0; i< windowSize; i++)
			tmpSmpWindow[i] *= filterWindowCoefficents[i];

		windowdSigSync->writeData(windowSize,tmpSmpWindow);

		for (uint i = 0; i < freqCount; i++) {
			dftResults[i] = 0;
			for (uint j=0; j < windowSize * windowMultiplier; j++) {
				if (padding == paddingTypeEnum::ZERO) {
					if (j < (windowSize))
						dftResults[i] += tmpSmpWindow[j] * dftMatrix[i][j];
					else
						dftResults[i] += 0;
				}
				else if (padding == paddingTypeEnum::SIG_REPEAT) //repeat samples
					dftResults[i] += tmpSmpWindow[j % windowSize] * dftMatrix[i][j];

			}
		}
	}

	void calcWindow(windowTypeEnum windowTypeIn) {

		if (windowTypeIn == windowTypeEnum::FLATTOP) {
			for (uint i = 0; i < windowSize; i++) {
				filterWindowCoefficents[i] = 	0.21557895
								- 0.41663158 * cos(2 * M_PI * i / (windowSize))
								+ 0.277263158 * cos(4 * M_PI * i / (windowSize))
								- 0.083578947 * cos(6 * M_PI * i / (windowSize))
								+ 0.006947368 * cos(8 * M_PI * i / (windowSize));
				windowCorretionFactor += filterWindowCoefficents[i];
			}
		}
		else if (windowTypeIn == windowTypeEnum::HAMMING || windowTypeIn == windowTypeEnum::HANN) {
			double a0 = 0.5; //this is the hann window
			if (windowTypeIn == windowTypeEnum::HAMMING)
				a0 = 25./46;

			for (uint i = 0; i < windowSize; i++) {
				filterWindowCoefficents[i] = a0 - (1 - a0) * cos(2 * M_PI * i / (windowSize));
				windowCorretionFactor += filterWindowCoefficents[i];
			}
		}
		else {
			for (uint i = 0; i < windowSize; i++) {
				filterWindowCoefficents[i] = 1;
				windowCorretionFactor += filterWindowCoefficents[i];
			}
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
