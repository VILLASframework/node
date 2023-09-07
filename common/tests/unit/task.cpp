/* Unit tests for periodic tasks.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <criterion/criterion.h>

#include <math.h>

#include <villas/task.hpp>
#include <villas/timing.hpp>

// cppcheck-suppress unknownMacro
TestSuite(task, .description = "Periodic timer tasks");

Test(task, rate, .timeout = 10) {
  int runs = 10;
  double rate = 5, waited;
  struct timespec start, end;
  Task task(CLOCK_MONOTONIC);

  task.setRate(rate);

  int i;
  for (i = 0; i < runs; i++) {
    clock_gettime(CLOCK_MONOTONIC, &start);

    task.wait();

    clock_gettime(CLOCK_MONOTONIC, &end);

    waited = time_delta(&start, &end);

    if (fabs(waited - 1.0 / rate) > 10e-3)
      break;
  }

  if (i < runs)
    cr_assert_float_eq(waited, 1.0 / rate, 1e-2,
                       "We slept for %f instead of %f secs in round %d", waited,
                       1.0 / rate, i);
}

Test(task, wait_until, .timeout = 5) {
  int ret;
  struct timespec start, end, diff, future;

  Task task(CLOCK_REALTIME);

  double waitfor = 3.423456789;

  start = time_now();
  diff = time_from_double(waitfor);
  future = time_add(&start, &diff);

  task.setNext(&future);

  ret = task.wait();

  end = time_now();

  cr_assert_eq(ret, 1);

  double waited = time_delta(&start, &end);

  cr_assert_float_eq(waited, waitfor, 1e-2,
                     "We slept for %f instead of %f secs", waited, waitfor);
}
