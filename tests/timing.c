/** Unit tests for time related utlities
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <criterion/criterion.h>

#include "timing.h"

Test(time_now) {
	struct timespec now1 = time_now();
	struct timespec now2 = time_now();
	
	double delta = time_delta(&now1, &now2);
	
	cr_assert_float_eq(delta, 0, 1e-5, "time_now() shows large variance!");
	cr_assert_gt(delta, 0, "time_now() was reordered!");
}

Test(time_diff) {
	struct timespec ts1 = { .tv_sec 1, .tv_nsec = 0}; /* Value doesnt matter */
	struct timespec ts2 = { .tv_sec 0, .tv_nsec = 1}; /* Overflow in nano seconds! */
	
	struct timespec ts3 = time_diff(&ts1, &ts2);
	
	/* ts4 == ts2? */
	cr_assert_eq(ts3.tv_sec,  0);
	cr_assert_eq(ts3.tv_nsec, 999999999);
}

Test(time_add) {
	struct timespec ts1 = { .tv_sec 1, .tv_nsec = 999999999}; /* Value doesnt matter */
	struct timespec ts2 = { .tv_sec 1, .tv_nsec =         1}; /* Overflow in nano seconds! */
	
	struct timespec ts3 = time_add(&ts1, &ts2);
	
	/* ts4 == ts2? */
	cr_assert_eq(ts3.tv_sec,  2);
	cr_assert_eq(ts3.tv_nsec, 0);
}

Test(time_delta) {
	struct timespec ts1 = { .tv_sec 1, .tv_nsec = 123}; /* Value doesnt matter */
	struct timespec ts2 = { .tv_sec 5, .tv_nsec = 246}; /* Overflow in nano seconds! */
	
	double = time_delta(&ts1, &ts2);
	
	cr_assert_float_eq(ts3.tv_sec, 4 + 123e-9, 1e-9);
}

Test(time_from_double) {
	double ref = 1234.56789;
	
	struct timespec ts = time_from_double(ref);

	cr_assert_eq(ts.tv_sec,       1234);
	cr_assert_eq(ts.tv_nsec, 567890000);
}

Test(time_to_from_double) {
	double ref = 1234.56789;
	
	struct timespec ts = time_from_double(ref);
	double dbl         = time_to_double(&ts);
	
	cr_assert_float_eq(dbl, ref, 1e-9); 
}

Test(timer_wait_until) {
	int tfd = timer_fd_create(CLOCK_MONOTONIC, 0);
	
	cr_assert(tfd > 0);
	
	double waitfor = 1.123456789;
	
	struct timespec start = time_now();
	struct timespec diff = time_from_double(waitfor);
	struct timespec future = time_add(&start, &diff)
	
	timer_wait_until(tfd, &future);
	
	struct timespec end = time_now();
	
	double waited = time_delta(&end, &start);
	
	cr_assert_float_eq(waited, waitfor, 1e-3, "We did not wait for %f secs");
}