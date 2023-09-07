/* Calc jitter, mean and variance of GPS vs NTP TS.
 *
 * Author: Umar Farooq <umar.farooq@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cinttypes>
#include <cstring>
#include <vector>

#include <villas/hook.hpp>
#include <villas/sample.hpp>
#include <villas/timing.hpp>
#include <villas/utils.hpp>

#define GPS_NTP_DELAY_WIN_SIZE 16

namespace villas {
namespace node {

class JitterCalcHook : public Hook {

protected:
  std::vector<int64_t> jitter_val;
  std::vector<int64_t> delay_series;
  std::vector<int64_t> moving_avg;
  std::vector<int64_t> moving_var;

  int64_t delay_mov_sum;
  int64_t delay_mov_sum_sqrd;
  int curr_count;

public:
  JitterCalcHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : Hook(p, n, fl, prio, en), jitter_val(GPS_NTP_DELAY_WIN_SIZE),
        delay_series(GPS_NTP_DELAY_WIN_SIZE),
        moving_avg(GPS_NTP_DELAY_WIN_SIZE), moving_var(GPS_NTP_DELAY_WIN_SIZE),
        delay_mov_sum(0), delay_mov_sum_sqrd(0), curr_count(0) {}

  /* Hook to calculate jitter between GTNET-SKT GPS timestamp and Villas node NTP timestamp.
	 *
	 * Drawbacks: No protection for out of order packets. Default positive delay assumed,
	 * so GPS timestamp should be earlier than NTP timestamp. If difference b/w NTP and GPS ts
	 * is high (i.e. several mins depending on GPS_NTP_DELAY_WIN_SIZE),
	 * the variance value will overrun the 64bit value.
	 */
  virtual Hook::Reason process(struct Sample *smp) {
    assert(state == State::STARTED);

    timespec now = time_now();
    int64_t delay_sec, delay_nsec, curr_delay_us;

    delay_sec = now.tv_sec - smp->ts.origin.tv_sec;
    delay_nsec = now.tv_nsec - smp->ts.origin.tv_nsec;

    /* Calc on microsec instead of nenosec delay as variance formula overflows otherwise.*/
    curr_delay_us = delay_sec * 1000000 + delay_nsec / 1000;

    delay_mov_sum = delay_mov_sum + curr_delay_us - delay_series[curr_count];
    moving_avg[curr_count] =
        delay_mov_sum /
        GPS_NTP_DELAY_WIN_SIZE; // Will be valid after GPS_NTP_DELAY_WIN_SIZE initial values

    delay_mov_sum_sqrd = delay_mov_sum_sqrd + (curr_delay_us * curr_delay_us) -
                         (delay_series[curr_count] * delay_series[curr_count]);
    moving_var[curr_count] =
        (delay_mov_sum_sqrd -
         (delay_mov_sum * delay_mov_sum) / GPS_NTP_DELAY_WIN_SIZE) /
        (GPS_NTP_DELAY_WIN_SIZE - 1);

    delay_series[curr_count] = curr_delay_us; // Update the last delay value

    /* Jitter calc formula as used in Wireshark according to RFC3550 (RTP)
			D(i,j) = (Rj-Ri)-(Sj-Si) = (Rj-Sj)-(Ri-Si)
			J(i) = J(i-1)+(|D(i-1,i)|-J(i-1))/16
		*/
    jitter_val[(curr_count + 1) % GPS_NTP_DELAY_WIN_SIZE] =
        jitter_val[curr_count] +
        (labs(curr_delay_us) - jitter_val[curr_count]) / 16;

    logger->info(
        "{}: jitter={} usec, moving average={} usec, moving variance={} usec",
        __FUNCTION__, jitter_val[(curr_count + 1) % GPS_NTP_DELAY_WIN_SIZE],
        moving_avg[curr_count], moving_var[curr_count]);

    curr_count++;
    if (curr_count >= GPS_NTP_DELAY_WIN_SIZE)
      curr_count = 0;

    return Reason::OK;
  }
};

// Register hook
static char n[] = "jitter_calc";
static char d[] = "Calc jitter, mean and variance of GPS vs NTP TS";
static HookPlugin<JitterCalcHook, n, d,
                  (int)Hook::Flags::NODE_READ | (int)Hook::Flags::PATH, 0>
    p;

} // namespace node
} // namespace villas
