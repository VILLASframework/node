/** Time related functions.
 *
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

#include <unistd.h>

#include <villas/timing.h>

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

ssize_t time_cmp(const struct timespec *a, const struct timespec *b)
{
	ssize_t sd = a->tv_sec - b->tv_sec;
	ssize_t nsd = a->tv_nsec - b->tv_nsec;

	return sd != 0 ? sd : nsd;
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
