/** PMU hook.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/dsp/window_cosine.hpp>
#include <villas/hook.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class PmuHook : public MultiSignalHook {


protected:
	enum class Status {
		INVALID,
		VALID
	};

	struct Phasor {
		double frequency;
		double amplitude;
		double phase;
		double rocof;	/* Rate of change of frequency. */
		Status valid;
	};

	enum class WindowType {
		NONE,
		FLATTOP,
		HANN,
		HAMMING,
		NUTTAL,
		BLACKMAN
	};

	enum class TimeAlign {
		LEFT,
		CENTER,
		RIGHT,
	};

	std::vector<dsp::CosineWindow<double>*> windows;
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
	/* Correction factors. */
	double phaseOffset;
	double amplitudeOffset;
	double frequencyOffset;
	double rocofOffset;

public:
	PmuHook(Path *p, Node *n, int fl, int prio, bool en = true);

	virtual
	void prepare();

	virtual
	void parse(json_t *json);

	virtual
	Hook::Reason process(struct Sample *smp);

	virtual
	Phasor estimatePhasor(dsp::CosineWindow<double> *window, Phasor lastPhasor);
};

} // namespace node
} // namespace villas
