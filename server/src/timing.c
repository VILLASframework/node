/** Time related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <unistd.h>
#include <sys/timerfd.h>

#include "timing.h"

uint64_t timerfd_wait(int fd)
{
	uint64_t runs;

	return read(fd, &runs, sizeof(runs)) < 0 ? 0 : runs;
}

uint64_t timerfd_wait_until(int fd, struct timespec *until)
{
	struct itimerspec its = {
		.it_value = *until,
		.it_interval = { 0, 0 }
	};

	if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &its, NULL))
		return 0;
	else
		return timerfd_wait(fd);
}

struct timespec time_add(struct timespec *start, struct timespec *end)
{
	struct timespec sum = {
		.tv_sec  = end->tv_sec  + start->tv_sec,
		.tv_nsec = end->tv_nsec + start->tv_nsec
	};

	if (sum.tv_nsec > 1000000000) {
		sum.tv_sec  += 1;
		sum.tv_nsec -= 1000000000;
	}

	return sum;
}

struct timespec time_diff(struct timespec *start, struct timespec *end)
{
	struct timespec diff = {
		.tv_sec  = end->tv_sec  - start->tv_sec,
		.tv_nsec = end->tv_nsec - start->tv_nsec
	};

	if (diff.tv_nsec < 0) {
		diff.tv_sec  -= 1;
		diff.tv_nsec += 1000000000;
	}

	return diff;
}

struct timespec time_from_double(double secs)
{
	struct timespec ts;

	ts.tv_sec  = secs;
	ts.tv_nsec = 1.0e9 * (secs - ts.tv_sec);

	return ts;
}

double time_to_double(struct timespec *ts)
{
	return ts->tv_sec + ts->tv_nsec * 1e-9;
}

double time_delta(struct timespec *start, struct timespec *end)
{
	struct timespec diff = time_diff(start, end);

	return time_to_double(&diff);
}

int time_fscan(FILE *f, struct timespec *ts)
{
	return fscanf(f, "%lu.%lu", &ts->tv_sec, &ts->tv_nsec);
}

int time_fprint(FILE *f, struct timespec *ts)
{
	return fprintf(f, "%lu.%09lu\t", ts->tv_sec, ts->tv_nsec);
}
