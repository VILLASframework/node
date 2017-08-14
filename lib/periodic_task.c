/** Run tasks periodically.
 *
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

#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "utils.h"

#include "periodic_task.h"
#include "timing.h"

#if PERIODIC_TASK_IMPL == TIMERFD
  #include <sys/timerfd.h>
#endif

int periodic_task_init(struct periodic_task *t, double rate)
{
	t->period = time_from_double(1.0 / rate);

#if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP || PERIODIC_TASK_IMPL == NANOSLEEP
	struct timespec now;
	
	clock_gettime(CLOCK_MONOTONIC, &now);
	
	t->next_period = time_add(&now, &t->period);
#elif PERIODIC_TASK_IMPL == TIMERFD
	int ret;

	struct itimerspec its = {
		.it_interval = t->period,
		.it_value = t->period
	};

	t->fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (t->fd < 0)
		return -1;

	ret = timerfd_settime(t->fd, 0, &its, NULL);
	if (ret)
		return ret;
#else
  #error "Invalid period task implementation"
#endif

	return 0;
}

int periodic_task_destroy(struct periodic_task *t)
{
#if PERIODIC_TASK_IMPL == TIMERFD
	return close(t->fd);
#endif
	
	return 0;
}

#if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP || PERIODIC_TASK_IMPL == NANOSLEEP
static int time_lt(const struct timespec *lhs, const struct timespec *rhs)
{
	if (lhs->tv_sec == rhs->tv_sec)
		return lhs->tv_nsec < rhs->tv_nsec;
	else
		return lhs->tv_sec < rhs->tv_sec;
	
	return 0;
}
#endif

uint64_t periodic_task_wait_until_next_period(struct periodic_task *t)
{
	uint64_t runs;
	int ret;

#if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP || PERIODIC_TASK_IMPL == NANOSLEEP
	ret = periodic_task_wait_until(t, &t->next_period);
	
	struct timespec now;
	
	ret = clock_gettime(CLOCK_MONOTONIC, &now);
	if (ret)
		return 0;
	
	for (runs = 0; time_lt(&t->next_period, &now); runs++)
		t->next_period = time_add(&t->next_period, &t->period);

#elif PERIODIC_TASK_IMPL == TIMERFD
	ret = read(t->fd, &runs, sizeof(runs));
	if (ret < 0)
		return 0;
#else
  #error "Invalid period task implementation"
#endif
	
	return runs;
}


int periodic_task_wait_until(struct periodic_task *t, const struct timespec *until)
{
	int ret;

#if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP
retry:	ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, until, NULL);
	if (ret == EINTR)
		goto retry;
#elif PERIODIC_TASK_IMPL == NANOSLEEP
	struct timespec now, delta;

	ret = clock_gettime(CLOCK_MONOTONIC, &now);
	if (ret)
		return ret;

	delta = time_diff(&now, until);

	ret = nanosleep(&delta, NULL);
#elif PERIODIC_TASK_IMPL == TIMERFD
	uint64_t runs;
	
	struct itimerspec its = {
		.it_value = *until,
		.it_interval = { 0, 0 }
	};

	ret = timerfd_settime(t->fd, TFD_TIMER_ABSTIME, &its, NULL);
	if (ret)
		return 0;

	ret = read(t->fd, &runs, sizeof(runs));
	if (ret < 0)
		return 0;
#else
  #error "Invalid period task implementation"
#endif
	
	return ret;
}