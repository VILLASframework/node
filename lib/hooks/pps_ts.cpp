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
#include <villas/log.h>

namespace villas {
namespace node {

class PpsTsHook : public Hook {

protected:
	double lastValue;
	double thresh;
	unsigned idx;
	uint64_t lastSequence;

	bool isSynced;
	bool isLocked;
	struct timespec tsVirt;
	double timeError;		/**< In seconds */
	double periodEst;		/**< In seconds */
	double periodErrComp;		/**< In seconds */
	double period;			/**< In seconds */
	uintmax_t cntEdges;
	uintmax_t cntSmps;
	uintmax_t cntSmpsTotal;
	unsigned horizonComp;
	unsigned horizonEst;
	std::vector<uintmax_t> filterWindow;

public:
	PpsTsHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		lastValue(0),
		thresh(1.5),
		idx(0),
		lastSequence(0),
		isSynced(false),
		isLocked(false),
		timeError(0.0),
		periodEst(0.0),
		periodErrComp(0.0),
		period(0.0),
		cntEdges(0),
		cntSmps(0),
		cntSmpsTotal(0),
		horizonComp(10),
		horizonEst(10),
		filterWindow(horizonEst + 1, 0)
	{ }

	virtual void parse(json_t *cfg)
	{
		int ret;
		json_error_t err;

		assert(state != State::STARTED);

		Hook::parse(cfg);

		double fSmps = 0;
		ret = json_unpack_ex(cfg, &err, 0, "{ s: i, s?: f, s: F}",
			"signal_index", &idx,
			"threshold", &thresh,
			"expected_smp_rate", &fSmps
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-pps_ts");

		period = 1.0 / fSmps;

		debug(LOG_HOOK | 5, "parsed config thresh=%f signal_index=%d nominal_period=%f", thresh, idx, period);


		state = State::PARSED;
	}

	virtual villas::node::Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);

		/* Get value of PPS signal */
		float value = smp->data[idx].f; // TODO check if it is really float

		/* Detect Edge */
		bool isEdge = lastValue < thresh && value > thresh;

		lastValue = value;

		if (isEdge) {
			if (isSynced) {
				if(tsVirt.tv_nsec > 0.5e9)
					timeError += 1.0 - (tsVirt.tv_nsec / 1.0e9);
				else
					timeError -= (tsVirt.tv_nsec / 1.0e9);


				filterWindow[cntEdges % filterWindow.size()] = cntSmpsTotal;
				/* Estimated sample period over last 'horizonEst' seconds */
				unsigned int tmp = cntEdges < filterWindow.size() ? cntEdges : horizonEst;
				double cntSmpsAvg = (cntSmpsTotal - filterWindow[(cntEdges - tmp) % filterWindow.size()]) / tmp;
				periodEst = 1.0 / cntSmpsAvg;
				periodErrComp = timeError / (cntSmpsAvg * horizonComp);
				period = periodEst + periodErrComp;
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

			debug(LOG_HOOK | 5, "Time Error is: %f periodEst %f periodErrComp %f", timeError, periodEst, periodErrComp);
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
			warning("Samples missed: %" PRIu64 " sampled missed", smp->sequence - lastSequence);

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
