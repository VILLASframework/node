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
	uint64_t last_sequence;

	bool isSynced;
	bool isLocked;
	struct timespec tsVirt;
	double timeErr;			// in seconds
	double periodEst;		// in seconds
	double periodErrComp;		// in seconds
	double period;			// in seconds
	uintmax_t cntEdges;
	uintmax_t cntSmps;
	uintmax_t cntSmpsTotal;
	unsigned horizonComp;
	unsigned horizonEst;
	uintmax_t filtLen;
	uintmax_t *filtWin;

public:
	PpsTsHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		lastValue(0),
		thresh(1.5),
		idx(0),
		last_sequence(0),
		isSynced(false),
		isLocked(false),
		timeErr(0.0),
		period(0.0),
		cntEdges(0),
		horizonComp(10),
		horizonEst(10),
		filtLen(horizonEst + 1)
	{
		filtWin = new uintmax_t[filtLen];
		memset(filtWin, 0, filtLen * sizeof(uintmax_t));
	}

	~PpsTsHook()
	{
		delete[] filtWin;
	}

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

		info("parsed config thresh=%f signal_index=%d nominal_period=%f", thresh, idx, period);


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
				//timeErr += 1.0 - (cntSmps * period);
				if(tsVirt.tv_nsec > 0.5e9)
					timeErr += 1.0 - (tsVirt.tv_nsec / 1.0e9);
				else
					timeErr -= (tsVirt.tv_nsec / 1.0e9);


				filtWin[cntEdges % filtLen] = cntSmpsTotal;
				/* Estimated sample period over last 'horizonEst' seconds */
				unsigned int tmp = cntEdges < filtLen ? cntEdges : horizonEst;
				double cntSmpsAvg = (cntSmpsTotal - filtWin[(cntEdges - tmp) % filtLen]) / tmp;
				periodEst = 1.0 / cntSmpsAvg;
				//info("cntSmpsAvg %f", cntSmpsAvg);
				periodErrComp = timeErr / (cntSmpsAvg * horizonComp);
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

			info("Time Error is: %f periodEst %f periodErrComp %f", timeErr, periodEst, periodErrComp);
		}

		cntSmps++;
		cntSmpsTotal++;

		if (cntEdges < 5)
			return Hook::Reason::SKIP_SAMPLE;

		smp->ts.origin = tsVirt;
		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;

		struct timespec tsPeriod = time_from_double(period);
		tsVirt = time_add(&tsVirt, &tsPeriod);


		if ((smp->sequence - last_sequence) > 1)
			warning("Samples missed: %li sampled missed", smp->sequence - last_sequence);

		last_sequence = smp->sequence;

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
