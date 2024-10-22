/* Unit tests for rdtsc.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <criterion/criterion.h>

#include <villas/timing.hpp>
#include <villas/tsc.h>
#include <villas/utils.hpp>

#define CNT (1 << 18)

// cppcheck-suppress unknownMacro
TestSuite(tsc, .description = "Timestamp counters");

Test(tsc, increasing) {
  int ret;
  struct Tsc tsc;
  uint64_t *cntrs;

  ret = tsc_init(&tsc);
  cr_assert_eq(ret, 0);

  cntrs = new uint64_t[CNT];
  cr_assert_not_null(cntrs);

  for (unsigned i = 0; i < CNT; i++)
    cntrs[i] = tsc_now(&tsc);

  for (unsigned i = 1; i < CNT; i++)
    cr_assert_lt(cntrs[i - 1], cntrs[i]);

  delete cntrs;
}

Test(tsc, sleep) {
  int ret;
  double delta, duration = 1;
  struct timespec start, stop;
  struct Tsc tsc;
  uint64_t start_cycles, end_cycles;

  ret = tsc_init(&tsc);
  cr_assert_eq(ret, 0);

  clock_gettime(CLOCK_MONOTONIC, &start);

  start_cycles = tsc_now(&tsc);
  end_cycles = start_cycles + duration * tsc.frequency;

  while (tsc_now(&tsc) < end_cycles)
    ;

  clock_gettime(CLOCK_MONOTONIC, &stop);
  delta = time_delta(&start, &stop);

  cr_assert_float_eq(delta, duration, 1e-4, "Error: %f, Delta: %lf, Freq: %llu",
                     delta - duration, delta, tsc.frequency);
}
