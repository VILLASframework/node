/** Run tasks periodically.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

#include <cstdio>
#include <cstdint>

#include <ctime>

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

#if PERIODIC_TASK_IMPL == RDTSC
  #include <villas/tsc.hpp>
#endif

struct Task {
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
	struct Tsc tsc;			/**< Initialized by tsc_init(). */
#endif

	/** Create a new task with the given rate. */
	Task(int clock = CLOCK_REALTIME);

	~Task();

	/** Wait until task elapsed
	 *
	 * @retval 0 An error occured. Maybe the task was stopped.
	 * @retval >0 The nummer of runs this task already fired.
	 */
	uint64_t wait();

	void setNext(const struct timespec *next);
	void setTimeout(double to);
	void setRate(double rate);

	void stop();

	/** Returns a poll'able file descriptor which becomes readable when the timer expires.
	 *
	 * Note: currently not supported on all platforms.
	 */
	int getFD() const;
};
