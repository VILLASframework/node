/** Unit tests for queue
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
#include <criterion/parameterized.h>

#include <villas/utils.h>
#include <villas/queue.h>
#include <villas/memory.h>
#include <villas/tsc.h>
#include <villas/log.hpp>

using namespace villas;

extern void init_memory();

#define SIZE	(1 << 10)

static struct queue q = { .state = ATOMIC_VAR_INIT(STATE_DESTROYED) };

#if defined(_POSIX_BARRIERS) && _POSIX_BARRIERS > 0
static pthread_barrier_t barrier;
#endif

struct param {
	int iter_count;
	int queue_size;
	int thread_count;
	bool many;
	int batch_size;
	enum memory_type_flags memory_type;
	volatile int start;
	struct queue queue;
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
	struct param *p = (struct param *) ctx;

	Logger logger = logging.get("test:queue:producer");

	srand((unsigned) time(0) + thread_get_id());
	size_t nops = rand() % 1000;

	//logger->info("tid = {}", thread_get_id());

#ifdef __APPLE__
  #define pthread_yield pthread_yield_np
#endif
	/* Wait for global start signal */
	while (p->start == 0)
		pthread_yield();

	//logger->info("wait for {} nops", nops);

	/* Wait for a random time */
	for (size_t i = 0; i != nops; i += 1)
		nop();

	//logger->info("start pushing");

	/* Enqueue */
	for (intptr_t count = 0; count < p->iter_count; count++) {
		do {
			ret = queue_push(&p->queue, (void *) count);
			pthread_yield();
		} while (ret != 1);
	}

	//logger->info("finished");

	return nullptr;
}

static void * consumer(void *ctx)
{
	int ret;
	struct param *p = (struct param *) ctx;

	srand((unsigned) time(0) + thread_get_id());
	size_t nops = rand() % 1000;

	Logger logger = logging.get("test:queue:consumer");

	//logger->info("tid = {}", thread_get_id());

	/* Wait for global start signal */
	while (p->start == 0)
		pthread_yield();

	//logger->info("wait for {} nops", nops);

	/* Wait for a random time */
	for (size_t i = 0; i != nops; i += 1)
		nop();

	//logger->info("start pulling");

	/* Dequeue */
	for (intptr_t count = 0; count < p->iter_count; count++) {
		intptr_t ptr;

		do {
			ret = queue_pull(&p->queue, (void **) &ptr);
		} while (ret != 1);

		//logger->info("consumer: {}", count);

		//cr_assert_eq((intptr_t) ptr, count);
	}

	//logger->info("finished");

	return nullptr;
}

#if defined(_POSIX_BARRIERS) && _POSIX_BARRIERS > 0
void * producer_consumer(void *ctx)
{
	struct param *p = (struct param *) ctx;

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

		for (intptr_t i = 0; i < p->batch_size; i++) {
			void *ptr = (void *) (iter * p->batch_size + i);
			while (!queue_push(&p->queue, ptr))
				pthread_yield(); /* queue full, let other threads proceed */
		}

		for (intptr_t i = 0; i < p->batch_size; i++) {
			void *ptr;
			while (!queue_pull(&p->queue, &ptr))
				pthread_yield(); /* queue empty, let other threads proceed */
		}
	}

	return 0;
}

void * producer_consumer_many(void *ctx)
{
	struct param *p = (struct param *) ctx;

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
		for (intptr_t i = 0; i < p->batch_size; i++)
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

Test(queue, single_threaded, .init = init_memory)
{
	int ret;
	struct param p;

	p.iter_count = 1 << 8;
	p.queue_size = 1 << 10;
	p.start = 1; /* we start immeadiatly */
	p.queue.state = ATOMIC_VAR_INIT(STATE_DESTROYED);

	ret = queue_init(&p.queue, p.queue_size, &memory_heap);
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
			.many = true,
			.batch_size = 10,
			.memory_type = MEMORY_HEAP
		}, {
			.iter_count = 1 << 8,
			.queue_size = 1 << 9,
			.thread_count = 4,
			.many = true,
			.batch_size = 100,
			.memory_type = MEMORY_HEAP
		}, {
			.iter_count = 1 << 16,
			.queue_size = 1 << 14,
			.thread_count = 16,
			.many = true,
			.batch_size = 100,
			.memory_type = MEMORY_HEAP
		}, {
			.iter_count = 1 << 8,
			.queue_size = 1 << 9,
			.thread_count = 4,
			.many = true,
			.batch_size = 10,
			.memory_type = MEMORY_HEAP
		}, {
			.iter_count = 1 << 16,
			.queue_size = 1 << 9,
			.thread_count = 16,
			.many = false,
			.batch_size = 10,
			.memory_type = MEMORY_HUGEPAGE
		}
	};

	return cr_make_param_array(struct param, params, ARRAY_LEN(params));
}

ParameterizedTest(struct param *p, queue, multi_threaded, .timeout = 20, .init = init_memory)
{
	int ret, cycpop;
	struct tsc tsc;

	Logger logger = logging.get("test:queue:multi_threaded");

	pthread_t threads[p->thread_count];

	p->start = 0;
	p->queue.state = ATOMIC_VAR_INIT(STATE_DESTROYED);

	struct memory_type *mt = memory_type_lookup(p->memory_type);

	ret = queue_init(&p->queue, p->queue_size, mt);
	cr_assert_eq(ret, 0, "Failed to create queue");

	uint64_t start_tsc_time, end_tsc_time;

	pthread_barrier_init(&barrier, nullptr, p->thread_count);

	for (int i = 0; i < p->thread_count; ++i)
		pthread_create(&threads[i], nullptr, p->many ? producer_consumer_many : producer_consumer, p);

	sleep(0.2);

	ret = tsc_init(&tsc);
	cr_assert(!ret);

	start_tsc_time = tsc_now(&tsc);
	p->start = 1;

	for (int i = 0; i < p->thread_count; ++i)
		pthread_join(threads[i], nullptr);

	end_tsc_time = tsc_now(&tsc);
	cycpop = (end_tsc_time - start_tsc_time) / p->iter_count;

	if (cycpop < 400)
		logger->debug("cycles/op: {}", cycpop);
	else
		logger->warn("cycles/op are very high ({}). Are you running on a hypervisor?", cycpop);

	ret = queue_available(&q);
	cr_assert_eq(ret, 0);

	ret = queue_destroy(&p->queue);
	cr_assert_eq(ret, 0, "Failed to destroy queue");

	ret = pthread_barrier_destroy(&barrier);
	cr_assert_eq(ret, 0, "Failed to destroy barrier");
}
#endif /* _POSIX_BARRIERS */

Test(queue, init_destroy, .init = init_memory)
{
	int ret;
	struct queue q;

	q.state = ATOMIC_VAR_INIT(STATE_DESTROYED);

	ret = queue_init(&q, 1024, &memory_heap);
	cr_assert_eq(ret, 0); /* Should succeed */

	ret = queue_destroy(&q);
	cr_assert_eq(ret, 0); /* Should succeed */
}
