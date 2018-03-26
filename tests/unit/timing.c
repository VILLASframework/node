/** Unit tests for time related utlities
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

#include <unistd.h>
#include <math.h>
#include <criterion/criterion.h>

#include <villas/timing.h>

Test(timing, time_now)
{
	struct timespec now1 = time_now();
	struct timespec now2 = time_now();

	double delta = time_delta(&now1, &now2);

	cr_assert_float_eq(delta, 0, 1e-5, "time_now() shows large variance!");
	cr_assert_gt(delta, 0, "time_now() was reordered!");
}

Test(timing, time_diff)
{
	struct timespec ts1 = { .tv_sec = 0, .tv_nsec = 1}; /* Value doesnt matter */
	struct timespec ts2 = { .tv_sec = 1, .tv_nsec = 0}; /* Overflow in nano seconds! */

	struct timespec ts3 = time_diff(&ts1, &ts2);

	/* ts4 == ts2? */
	cr_assert_eq(ts3.tv_sec,  0);
	cr_assert_eq(ts3.tv_nsec, 999999999);
}

Test(timing, time_add)
{
	struct timespec ts1 = { .tv_sec = 1, .tv_nsec = 999999999}; /* Value doesnt matter */
	struct timespec ts2 = { .tv_sec = 1, .tv_nsec =         1}; /* Overflow in nano seconds! */

	struct timespec ts3 = time_add(&ts1, &ts2);

	/* ts4 == ts2? */
	cr_assert_eq(ts3.tv_sec,  3);
	cr_assert_eq(ts3.tv_nsec, 0);
}

Test(timing, time_delta)
{
	struct timespec ts1 = { .tv_sec = 1, .tv_nsec = 123}; /* Value doesnt matter */
	struct timespec ts2 = { .tv_sec = 5, .tv_nsec = 246}; /* Overflow in nano seconds! */

	double delta = time_delta(&ts1, &ts2);

	cr_assert_float_eq(delta, 4 + 123e-9, 1e-9);
}

Test(timing, time_from_double)
{
	double ref = 1234.56789;

	struct timespec ts = time_from_double(ref);

	cr_assert_eq(ts.tv_sec,       1234);
	cr_assert_eq(ts.tv_nsec, 567890000);
}

Test(timing, time_to_from_double)
{
	double ref = 1234.56789;

	struct timespec ts = time_from_double(ref);
	double dbl         = time_to_double(&ts);

	cr_assert_float_eq(dbl, ref, 1e-9);
}
