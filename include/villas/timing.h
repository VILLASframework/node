/** Time related functions.
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
#include <sys/timerfd.h>

/** Create a new timer with the given rate. */
int timerfd_create_rate(double rate);

/** Wait until timer elapsed
 *
 * @param fd A file descriptor which was created by timerfd_create(3).
 * @retval 0 An error occured. Maybe the timer was stopped.
 * @retval >0 The nummer of runs this timer already fired.
 */
uint64_t timerfd_wait(int fd);

/** Wait until a fixed time in the future is reached
 *
 * @param fd A file descriptor which was created by timerfd_create(3).
 * @param until A pointer to a time in the future.
 * @retval 0 An error occured. Maybe the timer was stopped.
 * @retval >0 The nummer of runs this timer already fired.
 */
uint64_t timerfd_wait_until(int fd, struct timespec *until);

/** Get delta between two timespec structs */
struct timespec time_diff(struct timespec *start, struct timespec *end);

/** Get sum of two timespec structs */
struct timespec time_add(struct timespec *start, struct timespec *end);

/** Return current time as a struct timespec. */
struct timespec time_now();

/** Return the diffrence off two timestamps as double value in seconds. */
double time_delta(struct timespec *start, struct timespec *end);

/** Convert timespec to double value representing seconds */
double time_to_double(struct timespec *ts);

/** Convert double containing seconds after 1970 to timespec. */
struct timespec time_from_double(double secs);