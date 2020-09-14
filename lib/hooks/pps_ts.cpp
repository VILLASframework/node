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

class TimeSync {
protected:
    //Algorithms stability parameters
    //Examples from Paper:
    //const double p = 0.99;
    //const double k1 = 1.1;
    //const double k2 = 1.0;
    //Tuned parameters:
    const double p = 1.2;
    const double k1 = 1.3;
    const double k2 = 0.9;
    const double c = 0.7; // ~= mü_max
    const double tau = 1.; //Time between synchronizations
    //We assume no skew of our internal clock
    const double r = 1.;

    //Time estimate
    uint64_t x;
    //Skew correction
    double s;
    //Dampening Factor
    double y;
public:
    TimeSync() : x(0), s(1.), y(0.)
    {
        //Check stability
        assert( tau < (p*(k2-p*(k1-k2)))/
                (c*(k1-p*(k1-k2))*(k1-p*(k1-k2))) );
        assert( 2*k1/3*p > k1-k2 );
        assert( k1-k2 > 0 );
        assert( 2 > p && p > 0 );
    }

    /** Returns the current time Error
     *
     *  @actualTime The actual time from an external clock
     *  @nanoseconds the nanoseconds since the last synchronization event
     */
    int64_t timeError(const uint64_t actualTime, const uint64_t nanoseconds)
    {
        uint64_t currentEstimate = timeFromNanoseconds(nanoseconds);
        return (int64_t)actualTime + (int64_t)tau*1e9 - (int64_t)currentEstimate;
    }

    /** synchronizes with external clock
     *
     *  @actualTime nanoseconds at which event should occur
     *  @return nanoseconds estimate
     */
    uint64_t synchronize(const uint64_t actualTime, const uint64_t nanoseconds)
    {
        double t_err;
        uint64_t ret;
		//Time estimate
		x = timeFromNanoseconds(nanoseconds);
        //Time Error
        t_err = ((int64_t)actualTime + (int64_t)tau*1e9 - (int64_t)x)/1e9;
		//Skew correction or internal frequency / external frequency
		s = s + k1*(t_err) - k2*y;
		//Dampening Factor
		y = p*(t_err) + (1-p)*y;

        //Clip skew correction
        if (s > 1.1) {
            s = 1.1;
        } else if (s < 0.9) {
            s = 0.9;
        }
        printf("time error: %f, x: %lu, s: %f, y: %f\n", t_err, x, s, y);

        ret = x;
        //If time estimate gets too large, reset x
        if ( x > tau*1e9) {
            x -= tau*1e9;
        }
        return ret;
	}

    uint64_t time(const double partOfFullSecond)
    {
        return x + partOfFullSecond*tau*r*s*1e9;
    }

    /** Returns time estimate based on the nanoseconds that passed since
     *  last synchrsonization
     *
     *  @return nanoesecond estimate
     */
    uint64_t timeFromNanoseconds(const uint64_t n)
    {
        return time(n / 1.e9);
    }
};

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
	TimeSync ts;

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
		y(0),
		ts()
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

		auto now = time_now();
		realTime.tv_nsec = ts.timeFromNanoseconds(now.tv_nsec);

		if (realTime.tv_nsec >= 1e9) {
			realTime.tv_sec++;
			realTime.tv_nsec -= 1e9;
		}


		if (isEdge) {
			edgeCounter++;
			currentNanoseconds = now.tv_nsec;
			ts.synchronize(0.5e9, currentNanoseconds);
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
