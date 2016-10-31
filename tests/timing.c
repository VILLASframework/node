/** Unit tests for time related utlities
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <unistd.h>
#include <math.h>
#include <criterion/criterion.h>

#include "timing.h"

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

Test(timing, timerfd_create_rate, .timeout = 20)
{
	struct timespec start, end;
	
	double rate = 5, waited;
	
	int tfd = timerfd_create_rate(rate);
	
	cr_assert(tfd > 0);
	
	timerfd_wait(tfd);
	
	int i;
	for (i = 0; i < 10; i++) {
		start = time_now();

		timerfd_wait(tfd);
		
		end = time_now();
		waited = time_delta(&start, &end);
		
		if (fabs(waited - 1.0 / rate) > 10e-3)
			break;
	}
	
	if (i < 10)
		cr_assert_float_eq(waited, 1.0 / rate, 10e-3, "We slept for %f instead of %f secs in round %d", waited, 1.0 / rate, i);
	
	close(tfd);
}

Test(timing, timerfd_wait_until, .timeout = 1)
{
	int tfd = timerfd_create(CLOCK_REALTIME, 0);
	
	cr_assert(tfd > 0);
	
	double waitfor = 0.423456789;
	
	struct timespec start = time_now();
	struct timespec diff = time_from_double(waitfor);
	struct timespec future = time_add(&start, &diff);
	
	timerfd_wait_until(tfd, &future);
	
	struct timespec end = time_now();
	
	double waited = time_delta(&start, &end);
	
	cr_assert_float_eq(waited, waitfor, 5e-3, "We slept for %f instead of %f secs", waited, waitfor);
	
	close(tfd);
}