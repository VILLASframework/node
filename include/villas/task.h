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

/** We can choose between two periodic task implementations */
//#define PERIODIC_TASK_IMPL NANOSLEEP
#define TIMERFD		1
#define CLOCK_NANOSLEEP	2
#define NANOSLEEP	3

#if defined(__MACH__)
  #define PERIODIC_TASK_IMPL NANOSLEEP
#else
  #define PERIODIC_TASK_IMPL TIMERFD
#endif

struct task {
	struct timespec period;
#if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP || PERIODIC_TASK_IMPL == NANOSLEEP
	struct timespec next_period;
#elif PERIODIC_TASK_IMPL == TIMERFD
	int fd;
#else
  #error "Invalid period task implementation"
#endif
	int clock;
};

/** Create a new task with the given rate. */
int task_init(struct task *t, double rate, int clock);

int task_destroy(struct task *t);

/** Wait until task elapsed
 *
 * @retval 0 An error occured. Maybe the task was stopped.
 * @retval >0 The nummer of runs this task already fired.
 */
uint64_t task_wait_until_next_period(struct task *t);

/** Wait until a fixed time in the future is reached
 *
 * @param until A pointer to a time in the future.
 */
int task_wait_until(struct task *t, const struct timespec *until);

int task_fd(struct task *t);