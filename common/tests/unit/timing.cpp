/* Unit tests for time related utlities
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 */

#include <unistd.h>
#include <math.h>
#include <criterion/criterion.h>

#include <villas/timing.hpp>

// cppcheck-suppress unknownMacro
TestSuite(timing, .description = "Time measurements");

Test(timing, time_now)
{
	struct timespec now1 = time_now();
	struct timespec now2 = time_now();

	cr_assert(time_cmp(&now1, &now2) <= 0, "time_now() was reordered!");
}

Test(timing, time_diff)
{
	struct timespec ts1 = { .tv_sec = 0, .tv_nsec = 1}; // Value doesnt matter
	struct timespec ts2 = { .tv_sec = 1, .tv_nsec = 0}; // Overflow in nano seconds!

	struct timespec ts3 = time_diff(&ts1, &ts2);

	// ts4 == ts2?
	cr_assert_eq(ts3.tv_sec,  0);
	cr_assert_eq(ts3.tv_nsec, 999999999);
}

Test(timing, time_add)
{
	struct timespec ts1 = { .tv_sec = 1, .tv_nsec = 999999999}; // Value doesnt matter
	struct timespec ts2 = { .tv_sec = 1, .tv_nsec =         1}; // Overflow in nano seconds!

	struct timespec ts3 = time_add(&ts1, &ts2);

	// ts4 == ts2?
	cr_assert_eq(ts3.tv_sec,  3);
	cr_assert_eq(ts3.tv_nsec, 0);
}

Test(timing, time_delta)
{
	struct timespec ts1 = { .tv_sec = 1, .tv_nsec = 123}; // Value doesnt matter
	struct timespec ts2 = { .tv_sec = 5, .tv_nsec = 246}; // Overflow in nano seconds!

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
