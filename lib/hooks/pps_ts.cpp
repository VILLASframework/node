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
	double periodTime;//Period of sample frequency (Villas samples)
	unsigned idx;
	uint64_t lastSeqNr;
	unsigned edgeCounter;
	double pll_gain;
	timespec realTime;
	uint64_t last_sequence;
	double y;

public:
	PpsTsHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		lastValue(0),
		thresh(1.5),
		periodTime(1),
		idx(0),
		lastSeqNr(0),
		edgeCounter(0),
		pll_gain(1),
		realTime({ 0, 0 }),
		last_sequence(0),
		y(0)
	{
	}

	virtual void parse(json_t *cfg)
	{
		int ret;
		json_error_t err;

		assert(state != State::STARTED);

		Hook::parse(cfg);

		ret = json_unpack_ex(cfg, &err, 0, "{ s: i, s?: f, s?: f}",
			"signal_index", &idx,
			"threshold", &thresh,
			"pll_gain", &pll_gain
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
		uint64_t seqNr = smp->sequence;

		/* Detect Edge */
		bool isEdge = lastValue < thresh && value > thresh;
		lastValue = value;


		double timeErr;
		double changeVal;
		static const double targetVal = 0.5;


		realTime.tv_nsec += periodTime * 1e9 / 10000;

		if (realTime.tv_nsec >= 1e9) {
			realTime.tv_sec++;
			realTime.tv_nsec -= 1e9;
		}


		if (isEdge) {
			if (edgeCounter == 2) {
				auto now = time_now();

				if (now.tv_nsec >= 0.5e9)
					realTime.tv_sec = now.tv_sec + 1;
				else
					realTime.tv_sec = now.tv_sec;
				realTime.tv_nsec = targetVal * 1e9;
			}

			lastSeqNr = seqNr;
			edgeCounter++;

			timeErr = ( targetVal - (realTime.tv_nsec / 1e9) );
			if (edgeCounter >= 2){//pll mode
				
				changeVal = pll_gain * timeErr;

				double k2 = 1.;
				double p = 0.99;
				
				y = p * timeErr + ( 1 - p) * y;
				changeVal -= k2 * y;
 
				//changeVal = pll_gain;
				/*if(timeErr > 0 )
					periodTime -= changeVal;
				else if(timeErr < 0){*/
					periodTime += changeVal;
				//}
				double nominalSmpRate = 1;
				if(periodTime > 1.1/nominalSmpRate)periodTime = 1.1/nominalSmpRate;
				if(periodTime < 0.9/nominalSmpRate)periodTime = 0.9/nominalSmpRate;
			}
			//info("Edge detected: seq=%u, realTime.sec=%ld, realTime.nsec=%f, smpRate=%f, pll_gain=%f", seqNr, realTime.tv_sec, (1e9 - realTime.tv_nsec + 1e9/periodTime), periodTime, pll_gain);
		}




		
		if(isEdge){
			info("Edge detected: seq=%lu, realTime.nsec=%lu, timeErr=%f , timePeriod=%f, changeVal=%f", seqNr,realTime.tv_nsec,  timeErr, periodTime, changeVal);
		}

		if (edgeCounter < 2)
			return Hook::Reason::SKIP_SAMPLE;

		/* Update timestamp */
		smp->ts.origin = realTime;
		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;

		if((smp->sequence - last_sequence) > 1 )
			warning("Samples missed: %li sampled missed", smp->sequence - last_sequence);

		last_sequence = smp->sequence;

		return Hook::Reason::OK;
	}

	~PpsTsHook(){
	}
};

/* Register hook */
static char n[] = "pps_ts";
static char d[] = "Timestamp samples based GPS PPS signal";
static HookPlugin<PpsTsHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */
