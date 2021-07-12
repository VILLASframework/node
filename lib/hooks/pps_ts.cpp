/** Timestamp hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <cinttypes>
#include <vector>

#include <villas/hook.hpp>
#include <villas/timing.h>
#include <villas/sample.h>

namespace villas {
namespace node {

class PpsTsHook : public SingleSignalHook {

protected:
	double lastValue;
	double threshold;
	uint64_t lastSequence;

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
	std::vector<uintmax_t> filterWindow;

public:
	PpsTsHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		SingleSignalHook(p, n, fl, prio, en),
		lastValue(0),
		threshold(1.5),
		lastSequence(0),
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
		filterWindow(horizonEstimation + 1, 0)
	{ }

	virtual void parse(json_t *json)
	{
		int ret;
		json_error_t err;

		assert(state != State::STARTED);

		SingleSignalHook::parse(json);

		double fSmps = 0;
		ret = json_unpack_ex(json, &err, 0, "{ s?: f, s: F, s?: i, s?: i }",
			"threshold", &threshold,
			"expected_smp_rate", &fSmps,
			"horizon_estimation", &horizonEstimation,
			"horizon_compensation", &horizonCompensation

		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-pps_ts");

		period = 1.0 / fSmps;

		logger->debug("Parsed config threshold={} signal_index={} nominal_period={}", threshold, signalIndex, period);

		state = State::PARSED;
	}


	virtual villas::node::Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);
		/* Get value of PPS signal */
		float value = smp->data[signalIndex].f; // TODO check if it is really float
		/* Detect Edge */
		bool isEdge = lastValue < threshold && value > threshold;
		lastValue = value;
		if (isEdge) {
			tsVirt.tv_sec = time(nullptr);
			tsVirt.tv_nsec = 0;
			period = 1.0 / cntSmps;
			cntSmps = 0;
			cntEdges++;
		} else {
			struct timespec tsPeriod = time_from_double(period);
			tsVirt = time_add(&tsVirt, &tsPeriod);
		}

		cntSmps++;

		if (cntEdges < 5)
			return Hook::Reason::SKIP_SAMPLE;

		smp->ts.origin = tsVirt;
		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;

		if ((smp->sequence - lastSequence) > 1)
			logger->warn("Samples missed: {} sampled missed", smp->sequence - lastSequence);

		lastSequence = smp->sequence;
		return Hook::Reason::OK;
	}	

	virtual villas::node::Hook::Reason process_bak(sample *smp)
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

/** @} */
