/** PMU hook.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/hooks/pmu.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

PmuHook::PmuHook(Path *p, Node *n, int fl, int prio, bool en):
	MultiSignalHook(p, n, fl, prio, en),
	windows(),
	windowsTs(),
	timeAlignType(TimeAlign::CENTER),
	windowType(WindowType::NONE),
	sampleRate(1),
	phasorRate(1.0),
	nominalFreq(1.0),
	numberPlc(1.),
	windowSize(1),
	channelNameEnable(true),
	angleUnitFactor(1.0),
	lastSequence(0),
	nextRun({0}),
	init(false),
	initSampleCount(0),
	phaseOffset(0.0),
	amplitudeOffset(0.0),
	frequencyOffset(0.0),
	rocofOffset(0.0)
{ }

void PmuHook::prepare()
{
	MultiSignalHook::prepare();

	signals->clear();
	for (unsigned i = 0; i < signalIndices.size(); i++) {
		/* Add signals */
		auto freqSig = std::make_shared<Signal>("frequency", "Hz", SignalType::FLOAT);
		auto amplSig = std::make_shared<Signal>("amplitude", "V", SignalType::FLOAT);
		auto phaseSig = std::make_shared<Signal>("phase", (angleUnitFactor)?"rad":"deg", SignalType::FLOAT);//angleUnitFactor==1 means rad
		auto rocofSig = std::make_shared<Signal>("rocof", "Hz/s", SignalType::FLOAT);

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

		lastPhasors.push_back({0.,0.,0.,0.});
	}

	windowSize = ceil(sampleRate * numberPlc / nominalFreq);

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

void PmuHook::parse(json_t *json)
{
	MultiSignalHook::parse(json);
	int ret;

	const char *windowTypeC = nullptr;
	const char *angleUnitC = nullptr;
	const char *timeAlignC = nullptr;

	json_error_t err;

	assert(state != State::STARTED);

	Hook::parse(json);

	ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: F, s?: F, s?: F, s?: s, s?: s, s?: b, s?: s, s?: F, s?: F, s?: F, s?: F}",
		"sample_rate", &sampleRate,
		"dft_rate", &phasorRate,
		"nominal_freq", &nominalFreq,
		"number_plc", &numberPlc,
		"window_type", &windowTypeC,
		"angle_unit", &angleUnitC,
		"add_channel_name", &channelNameEnable,
		"timestamp_align", &timeAlignC,
		"phase_offset", &phaseOffset,
		"amplitude_offset", &amplitudeOffset,
		"frequency_offset", &frequencyOffset,
		"rocof_offset", &rocofOffset
	);

	if (ret)
		throw ConfigError(json, err, "node-config-hook-pmu");

	if (sampleRate <= 0)
		throw ConfigError(json, "node-config-hook-pmu-sample_rate", "Sample rate cannot be less than 0 tried to set {}", sampleRate);

	if (phasorRate <= 0)
		throw ConfigError(json, "node-config-hook-pmu-phasor_rate", "Phasor rate cannot be less than 0 tried to set {}", phasorRate);

	if (nominalFreq <= 0)
		throw ConfigError(json, "node-config-hook-pmu-nominal_freq", "Nominal frequency cannot be less than 0 tried to set {}", nominalFreq);

	if (numberPlc <= 0)
		throw ConfigError(json, "node-config-hook-pmu-number_plc", "Number of power line cycles cannot be less than 0 tried to set {}", numberPlc);

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
		throw ConfigError(json, "node-config-hook-pmu-window-type", "Invalid window type: {}", windowTypeC);

	if (!angleUnitC)
		logger->info("No angle type given, assume rad");
	else if (strcmp(angleUnitC, "rad") == 0)
		angleUnitFactor = 1;
	else if (strcmp(angleUnitC, "degree") == 0)
		angleUnitFactor = 180 / M_PI;
	else
		throw ConfigError(json, "node-config-hook-pmu-angle-unit", "Angle unit {} not recognized", angleUnitC);

	if (!timeAlignC)
		logger->info("No timestamp alignment defined. Assume alignment center");
	else if (strcmp(timeAlignC, "left") == 0)
		timeAlignType = TimeAlign::LEFT;
	else if (strcmp(timeAlignC, "center") == 0)
		timeAlignType = TimeAlign::CENTER;
	else if (strcmp(timeAlignC, "right") == 0)
		timeAlignType = TimeAlign::RIGHT;
	else
		throw ConfigError(json, "node-config-hook-dft-timestamp-alignment", "Timestamp alignment {} not recognized", timeAlignC);


}

Hook::Reason PmuHook::process(struct Sample *smp)
{
	assert(state == State::STARTED);

	initSampleCount++;

	if (smp->sequence - lastSequence > 1)
		logger->warn("Calculation is not Realtime. {} sampled missed", smp->sequence - lastSequence);
	lastSequence = smp->sequence;

	if (!init && initSampleCount > windowSize)
		init = true;

	timespec timeDiff = time_diff(&nextRun, &smp->ts.origin);
	double tmpTimeDiff = time_to_double(&timeDiff);
	bool run = false;
	if (tmpTimeDiff > 0. && init)
		run = true;

	Status phasorStatus = Status::VALID;
	timespec phasorTimestamp = {0};
	if (run) {
		for (unsigned i = 0; i < signalIndices.size(); i++) {
			lastPhasors[i] = estimatePhasor(windows[i], lastPhasors[i]);
			if (lastPhasors[i].valid != Status::VALID)
				phasorStatus = Status::INVALID;
		}

		double m = pow(10, floor(log10(phasorRate) + 1));
		if (phasorRate <= 1) // For less then 1 phasor per second
			m = pow(10, floor(log10(phasorRate)));
    		double nextRunDouble = (floor(time_to_double(&smp->ts.origin)*m)/m) + 1.0 / phasorRate;
		double r = fmod(nextRunDouble, 1 / phasorRate);
		if( (r > 1 / (4 * phasorRate)) && (r < 3 / (4 * phasorRate)))
			nextRunDouble += (1.0 / phasorRate) - fmod(nextRunDouble, 1 / phasorRate);
		nextRun = time_from_double(nextRunDouble);

		unsigned tsPos = 0;
		if (timeAlignType == TimeAlign::RIGHT)
			tsPos = windowSize;
		else if (timeAlignType == TimeAlign::LEFT)
			tsPos = 0;
		else if (timeAlignType == TimeAlign::CENTER)
			tsPos = windowSize / 2;
		phasorTimestamp = (*windowsTs)[tsPos];
	}

	/* Update sample memory */
	unsigned i = 0;
	for (auto index : signalIndices)
		windows[i++]->update(smp->data[index].f);
	windowsTs->update(smp->ts.origin);

	/* Make sure to update phasors after window update but estimate them before */
	if(run) {
		for (unsigned i = 0; i < signalIndices.size(); i++) {
			smp->data[i * 4 + 0].f = lastPhasors[i].frequency + frequencyOffset; /* Frequency */
			smp->data[i * 4 + 1].f = (lastPhasors[i].amplitude / pow(2, 0.5)) + amplitudeOffset; /* Amplitude */
			smp->data[i * 4 + 2].f = (lastPhasors[i].phase * 180 / M_PI) + phaseOffset; /* Phase */
			smp->data[i * 4 + 3].f = lastPhasors[i].rocof + rocofOffset; /* ROCOF */;
		}
		smp->ts.origin = phasorTimestamp;

		smp->length = signalIndices.size() * 4;
	}

	if (!run || phasorStatus != Status::VALID)
		return Reason::SKIP_SAMPLE;

	return Reason::OK;
}

PmuHook::Phasor PmuHook::estimatePhasor(dsp::CosineWindow<double> *window, Phasor lastPhasor)
{
	return {0., 0., 0., 0., Status::INVALID};
}

/* Register hook */
static char n[] = "pmu";
static char d[] = "This hook estimates a phsor";
static HookPlugin<PmuHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */
