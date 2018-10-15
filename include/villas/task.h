/** Run tasks periodically.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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
#include <villas/tsc.h>

#ifdef __cplusplus
extern "C" {
#endif

/** We can choose between two periodic task implementations */
//#define PERIODIC_TASK_IMPL NANOSLEEP
#define TIMERFD		1
#define CLOCK_NANOSLEEP	2
#define NANOSLEEP	3
#define RDTSC		4

#if defined(__MACH__)
  #define PERIODIC_TASK_IMPL NANOSLEEP
#elif defined(__linux__)
  #define PERIODIC_TASK_IMPL TIMERFD
#else
  #error "Platform not supported"
#endif

struct task {
	int clock;			/**< CLOCK_{MONOTONIC,REALTIME} */

#if PERIODIC_TASK_IMPL == RDTSC		/* We use cycle counts in RDTSC mode */
	uint64_t period;
	uint64_t next;
#else
	struct timespec period;		/**< The period of periodic invations of this task */
	struct timespec next;		/**< The timer value for the next invocation */
#endif

#if PERIODIC_TASK_IMPL == TIMERFD
	int fd;				/**< The timerfd_create(2) file descriptior. */
#elif PERIODIC_TASK_IMPL == RDTSC
	struct tsc tsc;		/**< Initialized by tsc_init(). */
#endif
};

/** Create a new task with the given rate. */
int task_init(struct task *t, double rate, int clock);

int task_destroy(struct task *t);

/** Wait until task elapsed
 *
 * @retval 0 An error occured. Maybe the task was stopped.
 * @retval >0 The nummer of runs this task already fired.
 */
uint64_t task_wait(struct task *t);

int task_set_next(struct task *t, struct timespec *next);
int task_set_timeout(struct task *t, double to);
int task_set_rate(struct task *t, double rate);

/** Returns a poll'able file descriptor which becomes readable when the timer expires.
 *
 * Note: currently not supported on all platforms.
 */
int task_fd(struct task *t);

#ifdef __cplusplus
}
#endif
