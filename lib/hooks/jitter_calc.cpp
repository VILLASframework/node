/** Calc jitter, mean and variance of GPS vs NTP TS.
 *
 * @author Umar Farooq <umar.farooq@rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <inttypes.h>
#include <string.h>

#include <villas/hook.hpp>
#include <villas/timing.h>
#include <villas/sample.h>
#include <villas/utils.hpp>

#define GPS_NTP_DELAY_WIN_SIZE 16

namespace villas {
namespace node {

class JitterCalcHook : public Hook {

protected:
	int64_t *jitter_val;
	int64_t *delay_series;
	int64_t *moving_avg;
	int64_t *moving_var;
	int64_t delay_mov_sum;
	int64_t delay_mov_sum_sqrd;
	int curr_count;

public:

	JitterCalcHook(struct path *p, struct node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en)
	{
		size_t sz = GPS_NTP_DELAY_WIN_SIZE;

		jitter_val	= new int64_t[sz];
		delay_series	= new int64_t[sz];
		moving_avg	= new int64_t[sz];
		moving_var	= new int64_t[sz];

		memset(jitter_val, 0, sz);
		memset(delay_series, 0, sz);
		memset(moving_avg, 0, sz);
		memset(moving_var, 0, sz);

		delay_mov_sum = 0;
		delay_mov_sum_sqrd = 0;
		curr_count = 0;
	}

	~JitterCalcHook()
	{
		delete jitter_val;
		delete delay_series;
		delete moving_avg;
		delete moving_var;
	}

	/**
	 * Hook to calculate jitter between GTNET-SKT GPS timestamp and Villas node NTP timestamp.
	 *
	 * Drawbacks: No protection for out of order packets. Default positive delay assumed,
	 * so GPS timestamp should be earlier than NTP timestamp. If difference b/w NTP and GPS ts
	 * is high (i.e. several mins depending on GPS_NTP_DELAY_WIN_SIZE),
	 * the variance value will overrun the 64bit value.
	 */
	virtual Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);

		timespec now = time_now();
		int64_t delay_sec, delay_nsec, curr_delay_us;

		delay_sec = now.tv_sec - smp->ts.origin.tv_sec;
		delay_nsec = now.tv_nsec - smp->ts.origin.tv_nsec;

		/* Calc on microsec instead of nenosec delay as variance formula overflows otherwise.*/
		curr_delay_us = delay_sec * 1000000 + delay_nsec / 1000;

		delay_mov_sum = delay_mov_sum + curr_delay_us - delay_series[curr_count];
		moving_avg[curr_count] = delay_mov_sum / GPS_NTP_DELAY_WIN_SIZE; /* Will be valid after GPS_NTP_DELAY_WIN_SIZE initial values */

		delay_mov_sum_sqrd = delay_mov_sum_sqrd + (curr_delay_us * curr_delay_us) - (delay_series[curr_count] * delay_series[curr_count]);
		moving_var[curr_count] = (delay_mov_sum_sqrd - (delay_mov_sum * delay_mov_sum) / GPS_NTP_DELAY_WIN_SIZE) / (GPS_NTP_DELAY_WIN_SIZE - 1);

		delay_series[curr_count] = curr_delay_us; /* Update the last delay value */

		/* Jitter calc formula as used in Wireshark according to RFC3550 (RTP)
			D(i,j) = (Rj-Ri)-(Sj-Si) = (Rj-Sj)-(Ri-Si)
			J(i) = J(i-1)+(|D(i-1,i)|-J(i-1))/16
		*/
		jitter_val[(curr_count + 1) % GPS_NTP_DELAY_WIN_SIZE] = jitter_val[curr_count] + (labs(curr_delay_us) - jitter_val[curr_count]) / 16;

		logger->info("{}: jitter={} usec, moving average={} usec, moving variance={} usec", __FUNCTION__, jitter_val[(curr_count + 1) % GPS_NTP_DELAY_WIN_SIZE], moving_avg[curr_count], moving_var[curr_count]);

		curr_count++;
		if (curr_count >= GPS_NTP_DELAY_WIN_SIZE)
			curr_count = 0;

		return Reason::OK;
	}
};

/* Register hook */
static HookPlugin<JitterCalcHook> p(
	"jitter_calc",
	"Calc jitter, mean and variance of GPS vs NTP TS",
	(int) Hook::Flags::NODE_READ | (int) Hook::Flags::PATH,
	0
);

} /* namespace node */
} /* namespace villas */

/** @} */
