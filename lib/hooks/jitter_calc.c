/** Calc jitter, mean and variance of GPS vs NTP TS.
 *
 * @author Umar Farooq <umar.farooq@rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include "hook.h"
#include "plugin.h"
#include "timing.h"

#define CALC_GPS_NTP_DELAY 0		/* @todo move to global config file */
#define GPS_NTP_DELAY_WIN_SIZE 16

static int64_t jitter_val[GPS_NTP_DELAY_WIN_SIZE] = {0};
static int64_t delay_series[GPS_NTP_DELAY_WIN_SIZE] = {0};
static int64_t moving_avg[GPS_NTP_DELAY_WIN_SIZE] = {0};
static int64_t moving_var[GPS_NTP_DELAY_WIN_SIZE] = {0};
static int64_t delay_mov_sum = 0, delay_mov_sum_sqrd = 0;
static int curr_count = 0;

/**
 * Hook to calculate jitter between GTNET-SKT GPS timestamp and Villas node NTP timestamp.
 *
 * Drawbacks: No protection for out of order packets. Default positive delay assumed,
 * so GPS timestamp should be earlier than NTP timestamp. If difference b/w NTP and GPS ts
 * is high (i.e. several mins depending on GPS_NTP_DELAY_WIN_SIZE),
 * the variance value will overrun the 64bit value.
 */
int hook_jitter_ts(struct hook *h, struct sample *smps[], size_t *cnt)
{
	/* @todo save data for each node, not just displaying on the screen, doesn't work for more than one node!!! */
	struct timespec now = time_now();
	int64_t delay_sec, delay_nsec, curr_delay_us;

	for(int i = 0; i < *cnt; i++) {
		delay_sec = now.tv_sec - smps[i]->ts.origin.tv_sec;
		delay_nsec = now.tv_nsec - smps[i]->ts.origin.tv_nsec;

		/* Calc on microsec instead of nenosec delay as variance formula overflows otherwise.*/
		curr_delay_us = delay_sec*1000000 + delay_nsec/1000;

		delay_mov_sum = delay_mov_sum + curr_delay_us - delay_series[curr_count];
		moving_avg[curr_count] = delay_mov_sum/(GPS_NTP_DELAY_WIN_SIZE); /* Will be valid after GPS_NTP_DELAY_WIN_SIZE initial values */

		delay_mov_sum_sqrd = delay_mov_sum_sqrd + (curr_delay_us*curr_delay_us) - (delay_series[curr_count]*delay_series[curr_count]);
		moving_var[curr_count] = (delay_mov_sum_sqrd - (delay_mov_sum*delay_mov_sum)/GPS_NTP_DELAY_WIN_SIZE)/(GPS_NTP_DELAY_WIN_SIZE-1);

		delay_series[curr_count] = curr_delay_us; /* Update the last delay value */

		/* Jitter calc formula as used in Wireshark according to RFC3550 (RTP)
			D(i,j) = (Rj-Ri)-(Sj-Si) = (Rj-Sj)-(Ri-Si)
			J(i) = J(i-1)+(|D(i-1,i)|-J(i-1))/16
		*/
		jitter_val[(curr_count+1)%GPS_NTP_DELAY_WIN_SIZE] = jitter_val[curr_count] + (abs(curr_delay_us) - jitter_val[curr_count])/16;

		info("jitter %ld usec, moving average %ld usec, moving variance %ld usec\n", jitter_val[(curr_count+1)%GPS_NTP_DELAY_WIN_SIZE], moving_avg[curr_count], moving_var[curr_count]);

		curr_count++;
		if(curr_count >= GPS_NTP_DELAY_WIN_SIZE)
			curr_count = 0;
	}
	return 0;
}

#if CALC_GPS_NTP_DELAY == 1
static struct plugin p = {
	.name		= "jitter_calc",
	.description	= "Calc jitter, mean and variance of GPS vs NTP TS",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 0,
		.builtin = true,
		.read	= hook_jitter_ts,
	}
};

REGISTER_PLUGIN(&p)
#endif

/** @} */
