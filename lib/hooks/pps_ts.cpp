/** Timestamp hook.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

#include <cinttypes>
#include <vector>

#include <villas/hook.hpp>
#include <villas/timing.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class PpsTsHook : public SingleSignalHook {

protected:
	enum Mode {
		SIMPLE,
		HORIZON,
	} mode;

	uint64_t lastSequence;

	double lastValue;
	double threshold;

	bool isSynced;
	bool isLocked;
	struct timespec tsVirt;
	double timeError;		/**< In seconds */
	double periodEstimate;		/**< In seconds */
	double periodErrorCompensation;	/**< In seconds */
	double period;			/**< In seconds */
	uintmax_t cntEdges;
	uintmax_t cntSmps;
	uintmax_t cntSmpsTotal;
	unsigned horizonCompensation;
	unsigned horizonEstimation;
	unsigned currentSecond;
	std::vector<uintmax_t> filterWindow;

public:
	PpsTsHook(Path *p, Node *n, int fl, int prio, bool en = true) :
		SingleSignalHook(p, n, fl, prio, en),
		mode(Mode::SIMPLE),
		lastSequence(0),
		lastValue(0),
		threshold(1.5),
		isSynced(false),
		isLocked(false),
		timeError(0.0),
		periodEstimate(0.0),
		periodErrorCompensation(0.0),
		period(0.0),
		cntEdges(0),
		cntSmps(0),
		cntSmpsTotal(0),
		horizonCompensation(10),
		horizonEstimation(10),
		currentSecond(0),
		filterWindow(horizonEstimation + 1, 0)
	{ }

	virtual void parse(json_t *json)
	{
		int ret;
		json_error_t err;

		assert(state != State::STARTED);

		SingleSignalHook::parse(json);

		const char *mode_str = nullptr;

		double fSmps = 0;
		ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: f, s: F, s?: i, s?: i }",
			"mode", &mode_str,
			"threshold", &threshold,
			"expected_smp_rate", &fSmps,
			"horizon_estimation", &horizonEstimation,
			"horizon_compensation", &horizonCompensation
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-pps_ts");

		period = 1.0 / fSmps;
		currentSecond = time(nullptr);

		if (mode_str) {
			if (!strcmp(mode_str, "simple"))
				mode = Mode::SIMPLE;
			else if (!strcmp(mode_str, "horizon"))
				mode = Mode::HORIZON;
			else
				throw ConfigError(json, "node-config-hook-pps_ts-mode", "Unsupported mode: {}", mode_str);
		}

		state = State::PARSED;
	}

	virtual villas::node::Hook::Reason process(struct Sample *smp)
	{
		switch (mode) {
			case Mode::SIMPLE:
				return processSimple(smp);

			case Mode::HORIZON:
				return processHorizon(smp);

			default:
				return Reason::ERROR;
		}
	}

	villas::node::Hook::Reason processSimple(struct Sample *smp)
	{
		assert(state == State::STARTED);

		/* Get value of PPS signal */
		float value = smp->data[signalIndex].f; // TODO check if it is really float

		/* Detect Edge */
		bool isEdge = lastValue < threshold && value > threshold;
		if (isEdge) {
			tsVirt.tv_sec = currentSecond + 1;
			tsVirt.tv_nsec = 0;
			period = 1.0 / cntSmps;
			cntSmps = 0;
			cntEdges++;
			currentSecond = 0;
		} else {
			struct timespec tsPeriod = time_from_double(period);
			tsVirt = time_add(&tsVirt, &tsPeriod);
		}

		lastValue = value;
		cntSmps++;

		if (!currentSecond && tsVirt.tv_nsec > 0.5e9)//take the second somewere in the center of the last second to reduce impact of system clock error
			currentSecond = time(nullptr);

		if (cntEdges < 5)
			return Hook::Reason::SKIP_SAMPLE;

		smp->ts.origin = tsVirt;
		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;

		if ((smp->sequence - lastSequence) > 1)
			logger->warn("Samples missed: {} sampled missed", smp->sequence - lastSequence);

		lastSequence = smp->sequence;
		return Hook::Reason::OK;
	}

	villas::node::Hook::Reason processHorizon(struct Sample *smp)
	{
		assert(state == State::STARTED);

		/* Get value of PPS signal */
		float value = smp->data[signalIndex].f; // TODO check if it is really float

		/* Detect Edge */
		bool isEdge = lastValue < threshold && value > threshold;

		lastValue = value;

		if (isEdge) {
			if (isSynced) {
				if(tsVirt.tv_nsec > 0.5e9)
					timeError += 1.0 - (tsVirt.tv_nsec / 1.0e9);
				else
					timeError -= (tsVirt.tv_nsec / 1.0e9);


				filterWindow[cntEdges % filterWindow.size()] = cntSmpsTotal;
				/* Estimated sample period over last 'horizonEstimation' seconds */
				unsigned int tmp = cntEdges < filterWindow.size() ? cntEdges : horizonEstimation;
				double cntSmpsAvg = (cntSmpsTotal - filterWindow[(cntEdges - tmp) % filterWindow.size()]) / tmp;
				periodEstimate = 1.0 / cntSmpsAvg;
				periodErrorCompensation = timeError / (cntSmpsAvg * horizonCompensation);
				period = periodEstimate + periodErrorCompensation;
			}
			else {
				tsVirt.tv_sec = time(nullptr);
				tsVirt.tv_nsec = 0;
				isSynced = true;
				cntEdges = 0;
				cntSmpsTotal = 0;
			}
			cntSmps = 0;
			cntEdges++;

			logger->debug("Time Error is: {} periodEstimate {} periodErrorCompensation {}", timeError, periodEstimate, periodErrorCompensation);
		}

		cntSmps++;
		cntSmpsTotal++;

		if (cntEdges < 5)
			return Hook::Reason::SKIP_SAMPLE;

		smp->ts.origin = tsVirt;
		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;

		struct timespec tsPeriod = time_from_double(period);
		tsVirt = time_add(&tsVirt, &tsPeriod);


		if ((smp->sequence - lastSequence) > 1)
			logger->warn("Samples missed: {} sampled missed", smp->sequence - lastSequence);

		lastSequence = smp->sequence;

		return Hook::Reason::OK;
	}
};

/* Register hook */
static char n[] = "pps_ts";
static char d[] = "Timestamp samples based GPS PPS signal";
static HookPlugin<PpsTsHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */
