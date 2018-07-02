/** Unit tests for queue
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <criterion/parameterized.h>

#include <villas/utils.h>
#include <villas/queue.h>
#include <villas/memory.h>

#define SIZE	(1 << 10)

static struct queue q = { .state = STATE_DESTROYED };

#if defined(_POSIX_BARRIERS) && _POSIX_BARRIERS > 0
static pthread_barrier_t barrier;
#endif

struct param {
	int volatile start;
	int thread_count;
	int queue_size;
	int iter_count;
	int batch_size;
	void * (*thread_func)(void *);
	struct queue queue;
	const struct memory_type *memory_type;
};

/** Get thread id as integer
 * In contrast to pthread_t which is an opaque type */
#ifdef __linux__
  #include <sys/syscall.h>
#endif

uint64_t thread_get_id()
{
#ifdef __MACH__
	uint64_t id;
	pthread_threadid_np(pthread_self(), &id);
	return id;
#elif defined(SYS_gettid)
	return (int) syscall(SYS_gettid);
#endif
	return -1;
}

/** Sleep, do nothing */
__attribute__((always_inline)) static inline void nop()
{
	__asm__("rep nop;");
}

static void * producer(void *ctx)
{
	int ret;
	struct param *p = ctx;

	srand((unsigned) time(0) + thread_get_id());
	size_t nops = rand() % 1000;

	/** @todo Criterion cr_log() is broken for multi-threaded programs */
	//cr_log_info("producer: tid = %lu", thread_get_id());

#ifdef __APPLE__
  #define pthread_yield pthread_yield_np
#endif
	/* Wait for global start signal */
	while (p->start == 0)
		pthread_yield();

	//cr_log_info("producer: wait for %zd nops", nops);

	/* Wait for a random time */
	for (size_t i = 0; i != nops; i += 1)
		nop();

	//cr_log_info("producer: start pushing");

	/* Enqueue */
	for (unsigned long count = 0; count < p->iter_count; count++) {
		do {
			ret = queue_push(&p->queue, (void *) count);
			pthread_yield();
		} while (ret != 1);
	}

	//cr_log_info("producer: finished");

	return NULL;
}

static void * consumer(void *ctx)
{
	int ret;
	struct param *p = ctx;

	srand((unsigned) time(0) + thread_get_id());
	size_t nops = rand() % 1000;

	//cr_log_info("consumer: tid = %lu", thread_get_id());

	/* Wait for global start signal */
	while (p->start == 0)
		pthread_yield();

	//cr_log_info("consumer: wait for %zd nops", nops);

	/* Wait for a random time */
	for (size_t i = 0; i != nops; i += 1)
		nop();

	//cr_log_info("consumer: start pulling");

	/* Dequeue */
	for (unsigned long count = 0; count < p->iter_count; count++) {
		void *ptr;

		do {
			ret = queue_pull(&p->queue, &ptr);
		} while (ret != 1);

		//cr_log_info("consumer: %lu\n", count);

		//cr_assert_eq((intptr_t) ptr, count);
	}

	//cr_log_info("consumer: finished");

	return NULL;
}

#if defined(_POSIX_BARRIERS) && _POSIX_BARRIERS > 0
void * producer_consumer(void *ctx)
{
	struct param *p = ctx;

	srand((unsigned) time(0) + thread_get_id());
	size_t nops = rand() % 1000;

	/* Wait for global start signal */
	while (p->start == 0)
		pthread_yield();

	/* Wait for a random time */
	for (size_t i = 0; i != nops; i += 1)
		nop();

	for (int iter = 0; iter < p->iter_count; ++iter) {
		pthread_barrier_wait(&barrier);

		for (size_t i = 0; i < p->batch_size; i++) {
			void *ptr = (void *) (iter * p->batch_size + i);
			while (!queue_push(&p->queue, ptr))
				pthread_yield(); /* queue full, let other threads proceed */
		}

		for (size_t i = 0; i < p->batch_size; i++) {
			void *ptr;
			while (!queue_pull(&p->queue, &ptr))
				pthread_yield(); /* queue empty, let other threads proceed */
		}
	}

	return 0;
}

void * producer_consumer_many(void *ctx)
{
	struct param *p = ctx;

	srand((unsigned) time(0) + thread_get_id());
	size_t nops = rand() % 1000;

	/* Wait for global start signal */
	while (p->start == 0)
		pthread_yield();

	/* Wait for a random time */
	for (size_t i = 0; i != nops; i += 1)
		nop();

	void *ptrs[p->batch_size];

	for (int iter = 0; iter < p->iter_count; ++iter) {
		for (size_t i = 0; i < p->batch_size; i++)
			ptrs[i] = (void *) (iter * p->batch_size + i);

		pthread_barrier_wait(&barrier);

		int pushed = 0;
		do {
			pushed += queue_push_many(&p->queue, &ptrs[pushed], p->batch_size - pushed);
			if (pushed != p->batch_size)
				pthread_yield(); /* queue full, let other threads proceed */
		} while (pushed < p->batch_size);

		int pulled = 0;
		do {
			pulled += queue_pull_many(&p->queue, &ptrs[pulled], p->batch_size - pulled);
			if (pulled != p->batch_size)
				pthread_yield(); /* queue empty, let other threads proceed */
		} while (pulled < p->batch_size);
	}

	return 0;
}
#endif /* _POSIX_BARRIERS */

Test(queue, single_threaded)
{
	int ret;
	struct param p = {
		.iter_count = 1 << 8,
		.queue_size = 1 << 10,
		.start = 1 /* we start immeadiatly */
	};

	ret = queue_init(&p.queue, p.queue_size, &memory_type_heap);
	cr_assert_eq(ret, 0, "Failed to create queue");

	producer(&p);
	consumer(&p);

	cr_assert_eq(queue_available(&q), 0);

	ret = queue_destroy(&p.queue);
	cr_assert_eq(ret, 0, "Failed to create queue");
}

#if defined(_POSIX_BARRIERS) && _POSIX_BARRIERS > 0
ParameterizedTestParameters(queue, multi_threaded)
{
	static struct param params[] = {
		{
			.iter_count = 1 << 12,
			.queue_size = 1 << 9,
			.thread_count = 32,
			.thread_func = producer_consumer_many,
			.batch_size = 10,
			.memory_type = &memory_type_heap
		}, {
			.iter_count = 1 << 8,
			.queue_size = 1 << 9,
			.thread_count = 4,
			.thread_func = producer_consumer_many,
			.batch_size = 100,
			.memory_type = &memory_type_heap
		}, {
			.iter_count = 1 << 16,
			.queue_size = 1 << 14,
			.thread_count = 16,
			.thread_func = producer_consumer_many,
			.batch_size = 100,
			.memory_type = &memory_type_heap
		}, {
			.iter_count = 1 << 8,
			.queue_size = 1 << 9,
			.thread_count = 4,
			.thread_func = producer_consumer_many,
			.batch_size = 10,
			.memory_type = &memory_type_heap
		}, {
			.iter_count = 1 << 16,
			.queue_size = 1 << 9,
			.thread_count = 16,
			.thread_func = producer_consumer,
			.batch_size = 10,
			.memory_type = &memory_hugepage
		}
	};

	return cr_make_param_array(struct param, params, ARRAY_LEN(params));
}

ParameterizedTest(struct param *p, queue, multi_threaded, .timeout = 20)
{
	int ret, cycpop;

	pthread_t threads[p->thread_count];

	p->start = 0;

	ret = queue_init(&p->queue, p->queue_size, &memory_type_heap);
	cr_assert_eq(ret, 0, "Failed to create queue");

	uint64_t start_tsc_time, end_tsc_time;

	pthread_barrier_init(&barrier, NULL, p->thread_count);

	for (int i = 0; i < p->thread_count; ++i)
		pthread_create(&threads[i], NULL, p->thread_func, p);

	sleep(0.2);

	start_tsc_time = rdtsc();
	p->start = 1;

	for (int i = 0; i < p->thread_count; ++i)
		pthread_join(threads[i], NULL);

	end_tsc_time = rdtsc();
	cycpop = (end_tsc_time - start_tsc_time) / p->iter_count;

	if (cycpop < 400)
		cr_log_info("cycles/op: %u\n", cycpop);
	else
		cr_log_warn("cycles/op are very high (%u). Are you running on a hypervisor?\n", cycpop);

	ret = queue_available(&q);
	cr_assert_eq(ret, 0);

	ret = queue_destroy(&p->queue);
	cr_assert_eq(ret, 0, "Failed to destroy queue");

	ret = pthread_barrier_destroy(&barrier);
	cr_assert_eq(ret, 0, "Failed to destroy barrier");
}
#endif /* _POSIX_BARRIERS */

Test(queue, init_destroy)
{
	int ret;
	struct queue q = { .state = STATE_DESTROYED };

	ret = queue_init(&q, 1024, &memory_type_heap);
	cr_assert_eq(ret, 0); /* Should succeed */

	ret = queue_destroy(&q);
	cr_assert_eq(ret, 0); /* Should succeed */
}
