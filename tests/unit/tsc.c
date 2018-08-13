/** Unit tests for rdtsc
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <criterion/criterion.h>

#include <villas/utils.h>
#include <villas/tsc.h>
#include <villas/timing.h>

#define CNT (1 << 18)

Test(tsc, increasing)
{
	int ret;
	struct tsc tsc;
	uint64_t *cntrs;

	ret = tsc_init(&tsc);
	cr_assert_eq(ret, 0);

	cntrs = alloc(sizeof(uint64_t) * CNT);
	cr_assert_not_null(cntrs);

	for (int i = 0; i < CNT; i++)
		cntrs[i] = tsc_now(&tsc);

	for (int i = 1; i < CNT; i++)
		cr_assert_lt(cntrs[i-1], cntrs[i]);

	free(cntrs);
}

Test(tsc, sleep)
{
	int ret;
	double delta, duration = 1;
	struct timespec start, stop;
	struct tsc tsc;
	uint64_t start_cycles, end_cycles;

	ret = tsc_init(&tsc);
	cr_assert_eq(ret, 0);

	clock_gettime(CLOCK_MONOTONIC, &start);

	start_cycles = tsc_now(&tsc);
	end_cycles = start_cycles + duration * tsc.frequency;

	while (tsc_now(&tsc) < end_cycles);

	clock_gettime(CLOCK_MONOTONIC, &stop);
	delta = time_delta(&start, &stop);

	cr_assert_float_eq(delta, duration, 1e-4, "Error: %f, Delta: %lf, Freq: %llu", delta - duration, delta, tsc.frequency);
}
