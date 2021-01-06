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
	double realSmpRate;
	unsigned idx;
	int lastSeqNr;
	unsigned edgeCounter;

	timespec realTime;

public:
	PpsTsHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		lastValue(0),
		thresh(1.5),
		realSmpRate(0),
		idx(0),
		lastSeqNr(0),
		edgeCounter(0),
		realTime({ 0, 0 })
	{
	}

	virtual void parse(json_t *cfg)
	{
        	int ret;
	        json_error_t err;

        	assert(state != State::STARTED);

		Hook::parse(cfg);

	        ret = json_unpack_ex(cfg, &err, 0, "{ s: i, s?: f }",
                	"signal_index", &idx,
			"threshold", &thresh
        	);
	        if (ret)
        	        throw ConfigError(cfg, err, "node-config-hook-pps_ts");

		info("parsed config thresh=%f signal_index=%d", thresh, idx);

	        state = State::PARSED;
	}

	virtual villas::node::Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);

		/* Get value of PPS signal */
		float value = smp->data[idx].f; // TODO check if it is really float
		int seqNr = smp->sequence;

		/* Detect Edge */
		bool isEdge = lastValue < thresh && value > thresh;
		if (isEdge) {
			if (edgeCounter >= 1)
				realSmpRate = seqNr - lastSeqNr;
			if (edgeCounter == 1) {
				auto now = time_now();

				if (now.tv_nsec >= 0.5e9)
					realTime.tv_sec = now.tv_sec + 1;
				else
					realTime.tv_sec = now.tv_sec;
			}

			lastSeqNr = seqNr;

			info("Edge detected: seq=%u, realTime.sec=%lld, realTime.nsec=%lld, smpRate=%f", seqNr, (long long) realTime.tv_sec,
													        (long long) realTime.tv_nsec,
													        realSmpRate);

			edgeCounter++;
		}

		lastValue = value;

		if (edgeCounter < 2)
			return Hook::Reason::SKIP_SAMPLE;
		else if (edgeCounter == 2 && isEdge)
			realTime.tv_nsec = 0;
		else
			realTime.tv_nsec += 1e9 / realSmpRate;

		if (realTime.tv_nsec >= 1000000000) {
			realTime.tv_sec++;
			realTime.tv_nsec -= 1000000000;
		}

		/* Update timestamp */
		smp->ts.origin = realTime;
		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;

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
