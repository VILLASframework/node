/** Run tasks periodically.
 *
 * @file
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

#pragma once

#include <stdio.h>
#include <stdint.h>

#include <time.h>

/** We can choose between two periodic timer implementations */
//#define PERIODIC_TASK_IMPL NANOSLEEP
#define TIMERFD		1
#define CLOCK_NANOSLEEP	2
#define NANOSLEEP	3

#if defined(__MACH__)
  #define PERIODIC_TASK_IMPL NANOSLEEP
#else
  #define PERIODIC_TASK_IMPL CLOCK_NANOSLEEP
#endif

struct periodic_task {
	struct timespec period;
#if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP || PERIODIC_TASK_IMPL == NANOSLEEP
	struct timespec next_period;
#elif PERIODIC_TASK_IMPL == TIMERFD
	int fd;
#else
  #error "Invalid period task implementation"
#endif
};

/** Create a new timer with the given rate. */
int periodic_task_init(struct periodic_task *t, double rate);

int periodic_task_destroy(struct periodic_task *t);

/** Wait until timer elapsed
 *
 * @retval 0 An error occured. Maybe the timer was stopped.
 * @retval >0 The nummer of runs this timer already fired.
 */
uint64_t periodic_task_wait_until_next_period(struct periodic_task *t);

/** Wait until a fixed time in the future is reached
 *
 * @param until A pointer to a time in the future.
 */
int periodic_task_wait_until(struct periodic_task *t, const struct timespec *until);