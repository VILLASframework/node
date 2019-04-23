/** Run tasks periodically.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#include <villas/utils.hpp>
#include <villas/task.h>
#include <villas/timing.h>

#if PERIODIC_TASK_IMPL == TIMERFD
  #include <sys/timerfd.h>
#endif /* PERIODIC_TASK_IMPL */

int task_init(struct task *t, double rate, int clock)
{
	int ret;

	t->clock = clock;

#if PERIODIC_TASK_IMPL == TIMERFD
	t->fd = timerfd_create(t->clock, 0);
	if (t->fd < 0)
		return -1;
#elif PERIODIC_TASK_IMPL == RDTSC
	ret = tsc_init(&t->tsc);
	if (ret)
		return ret;
#endif /* PERIODIC_TASK_IMPL */

	ret = task_set_rate(t, rate);
	if (ret)
		return ret;

	return 0;
}

int task_set_timeout(struct task *t, double to)
{
	struct timespec now;

	clock_gettime(t->clock, &now);

	struct timespec timeout = time_from_double(to);
	struct timespec next = time_add(&now, &timeout);

	return task_set_next(t, &next);
}

int task_set_next(struct task *t, struct timespec *next)
{

#if PERIODIC_TASK_IMPL != RDTSC
	t->next = *next;

  #if PERIODIC_TASK_IMPL == TIMERFD
	int ret;
	struct itimerspec its = {
		.it_interval = (struct timespec) { 0, 0 },
		.it_value = t->next
	};

	ret = timerfd_settime(t->fd, TFD_TIMER_ABSTIME, &its, nullptr);
	if (ret)
		return ret;
  #endif /* PERIODIC_TASK_IMPL == TIMERFD */
#endif /* PERIODIC_TASK_IMPL != RDTSC */

	return 0;
}

int task_set_rate(struct task *t, double rate)
{
#if PERIODIC_TASK_IMPL == RDTSC
	t->period = tsc_rate_to_cycles(&t->tsc, rate);
	t->next = tsc_now(&t->tsc) + t->period;
#else
	/* A rate of 0 will disarm the timer */
	t->period = rate ? time_from_double(1.0 / rate) : (struct timespec) { 0, 0 };

  #if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP || PERIODIC_TASK_IMPL == NANOSLEEP
	struct timespec now, next;

	clock_gettime(t->clock, &now);

	next = time_add(&now, &t->period);

	return task_set_next(t, &next);
  #elif PERIODIC_TASK_IMPL == TIMERFD
	int ret;
	struct itimerspec its = {
		.it_interval = t->period,
		.it_value = t->period
	};

	ret = timerfd_settime(t->fd, 0, &its, nullptr);
	if (ret)
		return ret;
  #endif /* PERIODIC_TASK_IMPL */
#endif /* PERIODIC_TASK_IMPL == RDTSC */

	return 0;
}

int task_destroy(struct task *t)
{
#if PERIODIC_TASK_IMPL == TIMERFD
	return close(t->fd);
#else
	return 0;
#endif
}

uint64_t task_wait(struct task *t)
{
	uint64_t runs;

#if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP || PERIODIC_TASK_IMPL == NANOSLEEP
	int ret;
	struct timespec now;

	ret = clock_gettime(t->clock, &now);
	if (ret)
		return ret;

	for (runs = 0; time_cmp(&t->next, &now) < 0; runs++)
		t->next = time_add(&t->next, &t->period);

  #if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP
	do {
		ret = clock_nanosleep(t->clock, TIMER_ABSTIME, &t->next, nullptr);
	} while (ret == EINTR);
  #elif PERIODIC_TASK_IMPL == NANOSLEEP
	struct timespec req, rem = time_diff(&now, &t->next);

	do {
		req = rem;
		ret = nanosleep(&req, &rem);
	} while (ret < 0 && errno == EINTR);
  #endif
	if (ret)
		return 0;

	ret = clock_gettime(t->clock, &now);
	if (ret)
		return ret;

	for (; time_cmp(&t->next, &now) < 0; runs++)
		t->next = time_add(&t->next, &t->period);
#elif PERIODIC_TASK_IMPL == TIMERFD
	int ret;

	ret = read(t->fd, &runs, sizeof(runs));
	if (ret < 0)
		return 0;
#elif PERIODIC_TASK_IMPL == RDTSC
	uint64_t now;

	do {
		now = tsc_now(&t->tsc);
	} while (now < t->next);


	for (runs = 0; t->next < now; runs++)
		t->next += t->period;
#else
  #error "Invalid period task implementation"
#endif

	return runs;
}

int task_fd(struct task *t)
{
#if PERIODIC_TASK_IMPL == TIMERFD
	return t->fd;
#else
	return -1;
#endif
}
