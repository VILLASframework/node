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

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/timing.h>
#include <villas/sample.h>

#define GPS_NTP_DELAY_WIN_SIZE 16

struct jitter_calc {
	int64_t *jitter_val;
	int64_t *delay_series;
	int64_t *moving_avg;
	int64_t *moving_var;
	int64_t delay_mov_sum;
	int64_t delay_mov_sum_sqrd;
	int curr_count;
};

int jitter_calc_init(struct hook *h)
{
	struct jitter_calc *j = (struct jitter_calc *) h->_vd;

	size_t sz = GPS_NTP_DELAY_WIN_SIZE * sizeof(int64_t);

	j->jitter_val	= alloc(sz);
	j->delay_series	= alloc(sz);
	j->moving_avg	= alloc(sz);
	j->moving_var	= alloc(sz);

	memset(j->jitter_val, 0, sz);
	memset(j->delay_series, 0, sz);
	memset(j->moving_avg, 0, sz);
	memset(j->moving_var, 0, sz);

	j->delay_mov_sum = 0;
	j->delay_mov_sum_sqrd = 0;
	j->curr_count = 0;

	return 0;
}

int jitter_calc_deinit(struct hook *h)
{
	struct jitter_calc *j = (struct jitter_calc *) h->_vd;

	free(j->jitter_val);
	free(j->delay_series);
	free(j->moving_avg);
	free(j->moving_var);

	return 0;
}

/**
 * Hook to calculate jitter between GTNET-SKT GPS timestamp and Villas node NTP timestamp.
 *
 * Drawbacks: No protection for out of order packets. Default positive delay assumed,
 * so GPS timestamp should be earlier than NTP timestamp. If difference b/w NTP and GPS ts
 * is high (i.e. several mins depending on GPS_NTP_DELAY_WIN_SIZE),
 * the variance value will overrun the 64bit value.
 */
static int jitter_calc_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct jitter_calc *j = (struct jitter_calc *) h->_vd;

	struct timespec now = time_now();
	int64_t delay_sec, delay_nsec, curr_delay_us;

	for(int i = 0; i < *cnt; i++) {
		delay_sec = now.tv_sec - smps[i]->ts.origin.tv_sec;
		delay_nsec = now.tv_nsec - smps[i]->ts.origin.tv_nsec;

		/* Calc on microsec instead of nenosec delay as variance formula overflows otherwise.*/
		curr_delay_us = delay_sec * 1000000 + delay_nsec / 1000;

		j->delay_mov_sum = j->delay_mov_sum + curr_delay_us - j->delay_series[j->curr_count];
		j->moving_avg[j->curr_count] = j->delay_mov_sum / GPS_NTP_DELAY_WIN_SIZE; /* Will be valid after GPS_NTP_DELAY_WIN_SIZE initial values */

		j->delay_mov_sum_sqrd = j->delay_mov_sum_sqrd + (curr_delay_us * curr_delay_us) - (j->delay_series[j->curr_count] * j->delay_series[j->curr_count]);
		j->moving_var[j->curr_count] = (j->delay_mov_sum_sqrd - (j->delay_mov_sum * j->delay_mov_sum) / GPS_NTP_DELAY_WIN_SIZE) / (GPS_NTP_DELAY_WIN_SIZE - 1);

		j->delay_series[j->curr_count] = curr_delay_us; /* Update the last delay value */

		/* Jitter calc formula as used in Wireshark according to RFC3550 (RTP)
			D(i,j) = (Rj-Ri)-(Sj-Si) = (Rj-Sj)-(Ri-Si)
			J(i) = J(i-1)+(|D(i-1,i)|-J(i-1))/16
		*/
		j->jitter_val[(j->curr_count + 1) % GPS_NTP_DELAY_WIN_SIZE] = j->jitter_val[j->curr_count] + (labs(curr_delay_us) - j->jitter_val[j->curr_count]) / 16;

		info("%s: jitter=%" PRId64 " usec, moving average=%" PRId64 " usec, moving variance=%" PRId64 " usec", __FUNCTION__, j->jitter_val[(j->curr_count + 1) % GPS_NTP_DELAY_WIN_SIZE], j->moving_avg[j->curr_count], j->moving_var[j->curr_count]);

		j->curr_count++;
		if (j->curr_count >= GPS_NTP_DELAY_WIN_SIZE)
			j->curr_count = 0;
	}

	return 0;
}

static struct plugin p = {
	.name		= "jitter_calc",
	.description	= "Calc jitter, mean and variance of GPS vs NTP TS",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_NODE_READ | HOOK_PATH,
		.priority	= 0,
		.init		= jitter_calc_init,
		.destroy	= jitter_calc_deinit,
		.process	= jitter_calc_process,
		.size		= sizeof(struct jitter_calc)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
