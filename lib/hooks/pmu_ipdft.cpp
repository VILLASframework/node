/** ipDFT PMU hook.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/hooks/pmu.hpp>

namespace villas {
namespace node {

class IpDftPmuHook : public PmuHook {

protected:
	std::complex<double> omega;
	std::vector<std::vector<std::complex<double>>> dftMatrix;
	std::vector<std::complex<double>> dftResult;

	unsigned frequencyCount; /* Number of requency bins that are calculated */
	double estimationRange; /* The range around nominalFreq used for estimation */

public:
	IpDftPmuHook(Path *p, Node *n, int fl, int prio, bool en = true):
		PmuHook(p, n, fl, prio, en),
		frequencyCount(0),
		estimationRange(0)

	{ }

	void prepare()
	{
		PmuHook::prepare();

		const double startFrequency = nominalFreq - estimationRange;
		const double endFrequency = nominalFreq + estimationRange;
		const double frequencyResolution = (double)sampleRate / windowSize;

		frequencyCount = ceil((endFrequency - startFrequency) / frequencyResolution);

		/* Initialize matrix of dft coeffients */
		dftMatrix.clear();
		for (unsigned i = 0; i < frequencyCount; i++)
			dftMatrix.emplace_back(windowSize, 0.0);

		using namespace std::complex_literals;

		omega = exp((-2i * M_PI) / (double)(windowSize));
		unsigned startBin = floor(startFrequency / frequencyResolution);

		for (unsigned i = 0; i <  frequencyCount ; i++) {
			for (unsigned j = 0 ; j < windowSize; j++)
				dftMatrix[i][j] = pow(omega, (i + startBin) * j);
			dftResult.push_back(0.0);
		}
	}

	void parse(json_t *json)
	{
		PmuHook::parse(json);
		int ret;

		json_error_t err;

		assert(state != State::STARTED);

		Hook::parse(json);

		ret = json_unpack_ex(json, &err, 0, "{ s?: F}",
			"estimation_range", &estimationRange
		);

		if (ret)
			throw ConfigError(json, err, "node-config-hook-ip-dft-pmu");

		if (estimationRange <= 0)
			throw ConfigError(json, "node-config-hook-ip-dft-pmu-estimation_range", "Estimation range cannot be less or equal than 0 tried to set {}", estimationRange);

	}

	PmuHook::Phasor estimatePhasor(dsp::CosineWindow<double> *window, PmuHook::Phasor lastPhasor)
	{
		PmuHook::Phasor phasor = {0};

		/* Calculate DFT */
		for (unsigned i = 0; i < frequencyCount; i++) {
			dftResult[i] = 0;

			const unsigned size = (*window).size();
			for (unsigned j = 0; j < size; j++)
				dftResult[i] += (*window)[j] * dftMatrix[i][j];
		}
		/* End calculate DFT */

		/* Find max bin */
		unsigned maxBin = 0;
		double absAmplitude = 0;

		for(unsigned j = 0; j < frequencyCount; j++) {
			if (absAmplitude < abs(dftResult[j])) {
				absAmplitude = abs(dftResult[j]);
				maxBin = j;
			}
		}
		/* End find max bin */

		if (maxBin == 0 || maxBin == (frequencyCount - 1)) {
			logger->warn("Maximum frequency bin lies on window boundary. Using non-estimated results!");
			//@todo add handling to not forward this phasor!!
		} else {
			const double startFrequency = nominalFreq - estimationRange;
			const double frequencyResolution = (double)sampleRate / windowSize;

			double a = abs(dftResult[ maxBin - 1 ]);
			double b = abs(dftResult[ maxBin + 0 ]);
			double c = abs(dftResult[ maxBin + 1 ]);
			double bPhase = atan2(dftResult[maxBin].imag(), dftResult[maxBin].real());

			/* Estimate phasor */
			/* Based on https://ieeexplore.ieee.org/document/7980868 */
			double delta = 0;

			/* Paper eq 8 */
			if (c > a) {
				delta = 1. * (2. * c - b) / (b + c);
			} else {
				delta = -1. * (2. * a - b) / (b + a);
			}

			/* Frequency estimation (eq 4) */
			phasor.frequency =  startFrequency + ( (double) maxBin + delta) * frequencyResolution;

			/* Amplitude estimation (eq 9) */
			phasor.amplitude = b * abs( (M_PI * delta) / sin( M_PI * delta) ) * abs( pow(delta, 2) - 1);
			phasor.amplitude *=  2 / (windowSize * window->getCorrectionFactor());

			/* Phase estimation (eq 10) */
			phasor.phase = bPhase - M_PI * delta;

			/* ROCOF estimation */
			phasor.rocof = ((phasor.frequency - lastPhasor.frequency) * (double)phasorRate);
			/* End estimate phasor */
		}

		if (lastPhasor.frequency != 0) /* Check if we already calculated a phasor before */
			phasor.valid = Status::VALID;

		return phasor;
	}
};

/* Register hook */
static char n[] = "ip-dft-pmu";
static char d[] = "This hook calculates a phasor based on ipDFT";
static HookPlugin<IpDftPmuHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */
