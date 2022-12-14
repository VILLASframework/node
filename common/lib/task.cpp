/** Run tasks periodically.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#include <unistd.h>
#include <ctime>
#include <cerrno>

#include <villas/utils.hpp>
#include <villas/task.hpp>
#include <villas/timing.hpp>
#include <villas/exceptions.hpp>

using namespace villas;

#if PERIODIC_TASK_IMPL == TIMERFD
  #include <sys/timerfd.h>
#endif // PERIODIC_TASK_IMPL

Task::Task(int clk) :
	clock(clk)
{
#if PERIODIC_TASK_IMPL == TIMERFD
	fd = timerfd_create(clock, 0);
	if (fd < 0)
		throw SystemError("Failed to create timerfd");
#elif PERIODIC_TASK_IMPL == RDTSC
	int ret = tsc_init(&tsc);
	if (ret)
		return ret;
#endif // PERIODIC_TASK_IMPL
}

void Task::setTimeout(double to)
{
	struct timespec now;

	clock_gettime(clock, &now);

	struct timespec timeout = time_from_double(to);
	struct timespec next = time_add(&now, &timeout);

	setNext(&next);
}

void Task::setNext(const struct timespec *nxt)
{

#if PERIODIC_TASK_IMPL != RDTSC
	next = *nxt;

  #if PERIODIC_TASK_IMPL == TIMERFD
	int ret;
	struct itimerspec its = {
		.it_interval = (struct timespec) { 0, 0 },
		.it_value = next
	};

	ret = timerfd_settime(fd, TFD_TIMER_ABSTIME, &its, nullptr);
	if (ret)
		throw SystemError("Failed to set timerfd");
  #endif // PERIODIC_TASK_IMPL == TIMERFD
#endif // PERIODIC_TASK_IMPL != RDTSC
}

void Task::setRate(double rate)
{
#if PERIODIC_TASK_IMPL == RDTSC
	period = tsc_rate_to_cycles(&tsc, rate);
	next = tsc_now(&tsc) + period;
#else
	// A rate of 0 will disarm the timer
	period = rate ? time_from_double(1.0 / rate) : (struct timespec) { 0, 0 };

  #if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP || PERIODIC_TASK_IMPL == NANOSLEEP
	struct timespec now, next;

	clock_gettime(clock, &now);

	next = time_add(&now, &period);

	return setNext(&next);
  #elif PERIODIC_TASK_IMPL == TIMERFD
	int ret;
	struct itimerspec its = {
		.it_interval = period,
		.it_value = period
	};

	ret = timerfd_settime(fd, 0, &its, nullptr);
	if (ret)
		throw SystemError("Failed to set timerfd");
  #endif // PERIODIC_TASK_IMPL
#endif // PERIODIC_TASK_IMPL == RDTSC
}

Task::~Task()
{
#if PERIODIC_TASK_IMPL == TIMERFD
	close(fd);
#endif
}

uint64_t Task::wait()
{
	uint64_t runs;

#if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP || PERIODIC_TASK_IMPL == NANOSLEEP
	int ret;
	struct timespec now;

	ret = clock_gettime(clock, &now);
	if (ret)
		return ret;

	for (runs = 0; time_cmp(&next, &now) < 0; runs++)
		next = time_add(&next, &period);

  #if PERIODIC_TASK_IMPL == CLOCK_NANOSLEEP
	do {
		ret = clock_nanosleep(clock, TIMER_ABSTIME, &next, nullptr);
	} while (ret == EINTR);
  #elif PERIODIC_TASK_IMPL == NANOSLEEP
	struct timespec req, rem = time_diff(&now, &next);

	do {
		req = rem;
		ret = nanosleep(&req, &rem);
	} while (ret < 0 && errno == EINTR);
  #endif
	if (ret)
		return 0;

	ret = clock_gettime(clock, &now);
	if (ret)
		return ret;

	for (; time_cmp(&next, &now) < 0; runs++)
		next = time_add(&next, &period);
#elif PERIODIC_TASK_IMPL == TIMERFD
	int ret;

	ret = read(fd, &runs, sizeof(runs));
	if (ret < 0)
		return 0;
#elif PERIODIC_TASK_IMPL == RDTSC
	uint64_t now;

	do {
		now = tsc_now(&tsc);
	} while (now < next);


	for (runs = 0; next < now; runs++)
		next += period;
#else
  #error "Invalid period task implementation"
#endif

	return runs;
}

void Task::stop()
{
#if PERIODIC_TASK_IMPL == TIMERFD
	int ret;
	struct itimerspec its = {
		.it_interval = (struct timespec) { 0, 0 },
		.it_value = (struct timespec) { 0, 0 }
	};

	ret = timerfd_settime(fd, 0, &its, nullptr);
	if (ret)
		throw SystemError("Failed to disarm timerfd");
#endif // PERIODIC_TASK_IMPL == TIMERFD
}

int Task::getFD() const
{
#if PERIODIC_TASK_IMPL == TIMERFD
	return fd;
#else
	return -1;
#endif
}
