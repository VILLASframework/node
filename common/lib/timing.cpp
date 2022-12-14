/** Time related functions.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#include <unistd.h>

#include <villas/timing.hpp>

struct timespec time_now()
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	return ts;
}

struct timespec time_add(const struct timespec *start, const struct timespec *end)
{
	struct timespec sum = {
		.tv_sec  = end->tv_sec  + start->tv_sec,
		.tv_nsec = end->tv_nsec + start->tv_nsec
	};

	if (sum.tv_nsec >= 1000000000) {
		sum.tv_sec  += 1;
		sum.tv_nsec -= 1000000000;
	}

	return sum;
}

struct timespec time_diff(const struct timespec *start, const struct timespec *end)
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

double time_to_double(const struct timespec *ts)
{
	return ts->tv_sec + ts->tv_nsec * 1e-9;
}

double time_delta(const struct timespec *start, const struct timespec *end)
{
	struct timespec diff = time_diff(start, end);

	return time_to_double(&diff);
}

ssize_t time_cmp(const struct timespec *a, const struct timespec *b)
{
	ssize_t sd = a->tv_sec - b->tv_sec;
	ssize_t nsd = a->tv_nsec - b->tv_nsec;

	return sd != 0 ? sd : nsd;
}
