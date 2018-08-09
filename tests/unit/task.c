/** Unit tests for periodic tasks
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <criterion/criterion.h>

#include <math.h>

#include <villas/task.h>
#include <villas/timing.h>

Test(task, rate, .timeout = 10)
{
	int ret;
	int runs = 10;
	double rate = 5, waited;
	struct timespec start, end;
	struct task task;

	ret = task_init(&task, rate, CLOCK_MONOTONIC);
	cr_assert_eq(ret, 0);

	int i;
	for (i = 0; i < runs; i++) {
		clock_gettime(CLOCK_MONOTONIC, &start);

		task_wait(&task);

		clock_gettime(CLOCK_MONOTONIC, &end);

		waited = time_delta(&start, &end);

		if (fabs(waited - 1.0 / rate) > 10e-3)
			break;
	}

	if (i < runs)
		cr_assert_float_eq(waited, 1.0 / rate, 1e-2, "We slept for %f instead of %f secs in round %d", waited, 1.0 / rate, i);

	ret = task_destroy(&task);
	cr_assert_eq(ret, 0);
}

Test(task, wait_until, .timeout = 5)
{
	int ret;
	struct task task;
	struct timespec start, end, diff, future;

	ret = task_init(&task, 1, CLOCK_REALTIME);
	cr_assert_eq(ret, 0);

	double waitfor = 3.423456789;

	start = time_now();
	diff = time_from_double(waitfor);
	future = time_add(&start, &diff);

	ret = task_set_next(&task, &future);
	cr_assert_eq(ret, 0);

	ret = task_wait(&task);

	end = time_now();

	cr_assert_eq(ret, 1);

	double waited = time_delta(&start, &end);

	cr_assert_float_eq(waited, waitfor, 1e-2, "We slept for %f instead of %f secs", waited, waitfor);

	ret = task_destroy(&task);
	cr_assert_eq(ret, 0);
}
