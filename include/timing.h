/** Time related functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _TIME_H_
#define _TIME_H_

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

#endif /* _TIMING_H_ */