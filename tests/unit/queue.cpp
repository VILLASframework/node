/* Unit tests for queue.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <villas/log.hpp>
#include <villas/node/memory.hpp>
#include <villas/queue.h>
#include <villas/tsc.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;

extern void init_memory();

#define SIZE (1 << 10)

static struct CQueue q;

#if defined(_POSIX_BARRIERS) && _POSIX_BARRIERS > 0
static pthread_barrier_t barrier;
#endif

struct param {
  int iter_count;
  int queue_size;
  int thread_count;
  bool many;
  int batch_size;
  struct memory::Type *mt;
  volatile int start;
  struct CQueue queue;
};

/* Get thread id as integer
 * In contrast to pthread_t which is an opaque type */
#ifdef __linux__
#include <sys/syscall.h>
#endif

uint64_t thread_get_id() {
#ifdef __MACH__
  uint64_t id;
  pthread_threadid_np(pthread_self(), &id);
  return id;
#elif defined(SYS_gettid)
  return (int)syscall(SYS_gettid);
#endif
  return -1;
}

// Sleep, do nothing
__attribute__((always_inline)) static inline void nop() { __asm__("rep nop;"); }

static void *producer(void *ctx) {
  int ret;
  struct param *p = (struct param *)ctx;

  srand((unsigned)time(0) + thread_get_id());
  size_t nops = rand() % 1000;

  // Wait for global start signal
  while (p->start == 0)
    sched_yield();

  // Wait for a random time
  for (size_t i = 0; i != nops; i += 1)
    nop();

  // Enqueue
  for (intptr_t count = 0; count < p->iter_count; count++) {
    do {
      ret = queue_push(&p->queue, (void *)count);
      sched_yield();
    } while (ret != 1);
  }

  return nullptr;
}

static void *consumer(void *ctx) {
  int ret;
  struct param *p = (struct param *)ctx;

  srand((unsigned)time(0) + thread_get_id());
  size_t nops = rand() % 1000;

  // Wait for global start signal
  while (p->start == 0)
    sched_yield();

  // Wait for a random time
  for (size_t i = 0; i != nops; i += 1)
    nop();

  // Dequeue
  for (intptr_t count = 0; count < p->iter_count; count++) {
    intptr_t ptr;

    do {
      ret = queue_pull(&p->queue, (void **)&ptr);
    } while (ret != 1);

    //logger->info("consumer: {}", count);

    //cr_assert_eq((intptr_t) ptr, count);
  }

  return nullptr;
}

#if defined(_POSIX_BARRIERS) && _POSIX_BARRIERS > 0
void *producer_consumer(void *ctx) {
  struct param *p = (struct param *)ctx;

  srand((unsigned)time(0) + thread_get_id());
  size_t nops = rand() % 1000;

  // Wait for global start signal
  while (p->start == 0)
    sched_yield();

  // Wait for a random time
  for (size_t i = 0; i != nops; i += 1)
    nop();

  for (int iter = 0; iter < p->iter_count; ++iter) {
    pthread_barrier_wait(&barrier);

    for (intptr_t i = 0; i < p->batch_size; i++) {
      void *ptr = (void *)(iter * p->batch_size + i);
      while (!queue_push(&p->queue, ptr))
        sched_yield(); // queue full, let other threads proceed
    }

    for (intptr_t i = 0; i < p->batch_size; i++) {
      void *ptr;
      while (!queue_pull(&p->queue, &ptr))
        sched_yield(); // queue empty, let other threads proceed
    }
  }

  return 0;
}

void *producer_consumer_many(void *ctx) {
  struct param *p = (struct param *)ctx;

  srand((unsigned)time(0) + thread_get_id());
  size_t nops = rand() % 1000;

  // Wait for global start signal
  while (p->start == 0)
    sched_yield();

  // Wait for a random time
  for (size_t i = 0; i != nops; i += 1)
    nop();

  void *ptrs[p->batch_size];

  for (int iter = 0; iter < p->iter_count; ++iter) {
    for (intptr_t i = 0; i < p->batch_size; i++)
      ptrs[i] = (void *)(iter * p->batch_size + i);

    pthread_barrier_wait(&barrier);

    int pushed = 0;
    do {
      pushed +=
          queue_push_many(&p->queue, &ptrs[pushed], p->batch_size - pushed);
      if (pushed != p->batch_size)
        sched_yield(); // queue full, let other threads proceed
    } while (pushed < p->batch_size);

    int pulled = 0;
    do {
      pulled +=
          queue_pull_many(&p->queue, &ptrs[pulled], p->batch_size - pulled);
      if (pulled != p->batch_size)
        sched_yield(); // queue empty, let other threads proceed
    } while (pulled < p->batch_size);
  }

  return 0;
}
#endif // _POSIX_BARRIERS

Test(queue, single_threaded, .init = init_memory) {
  int ret;
  struct param p;

  p.iter_count = 1 << 8;
  p.queue_size = 1 << 10;
  p.start = 1; // we start immeadiatly

  ret = queue_init(&p.queue, p.queue_size, &memory::heap);
  cr_assert_eq(ret, 0, "Failed to create queue");

  producer(&p);
  consumer(&p);

  cr_assert_eq(queue_available(&q), 0);

  ret = queue_destroy(&p.queue);
  cr_assert_eq(ret, 0, "Failed to create queue");
}

#if defined(_POSIX_BARRIERS) && _POSIX_BARRIERS > 0
ParameterizedTestParameters(queue, multi_threaded) {
  static struct param params[] = {{.iter_count = 1 << 12,
                                   .queue_size = 1 << 9,
                                   .thread_count = 32,
                                   .many = true,
                                   .batch_size = 10,
                                   .mt = &memory::heap},
                                  {.iter_count = 1 << 8,
                                   .queue_size = 1 << 9,
                                   .thread_count = 4,
                                   .many = true,
                                   .batch_size = 100,
                                   .mt = &memory::heap},
                                  {.iter_count = 1 << 16,
                                   .queue_size = 1 << 14,
                                   .thread_count = 16,
                                   .many = true,
                                   .batch_size = 100,
                                   .mt = &memory::heap},
                                  {.iter_count = 1 << 8,
                                   .queue_size = 1 << 9,
                                   .thread_count = 4,
                                   .many = true,
                                   .batch_size = 10,
                                   .mt = &memory::heap},
                                  {.iter_count = 1 << 16,
                                   .queue_size = 1 << 9,
                                   .thread_count = 16,
                                   .many = false,
                                   .batch_size = 10,
                                   .mt = &memory::mmap_hugetlb}};

  return cr_make_param_array(struct param, params, ARRAY_LEN(params));
}

ParameterizedTest(struct param *p, queue, multi_threaded, .timeout = 20,
                  .init = init_memory) {
  int ret, cycpop;
  struct Tsc tsc;

  Logger logger = logging.get("test:queue:multi_threaded");

  if (!utils::isPrivileged() && p->mt == &memory::mmap_hugetlb)
    cr_skip_test("Skipping memory_mmap_hugetlb tests allocatpr because we are "
                 "running in an unprivileged environment.");

  pthread_t threads[p->thread_count];

  p->start = 0;

  ret = queue_init(&p->queue, p->queue_size, p->mt);
  cr_assert_eq(ret, 0, "Failed to create queue");

  uint64_t start_tsc_time, end_tsc_time;

  pthread_barrier_init(&barrier, nullptr, p->thread_count);

  for (int i = 0; i < p->thread_count; ++i)
    pthread_create(&threads[i], nullptr,
                   p->many ? producer_consumer_many : producer_consumer, p);

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
    logger->debug("Cycles/op: {}", cycpop);
  else
    logger->warn(
        "Cycles/op are very high ({}). Are you running on a hypervisor?",
        cycpop);

  ret = queue_available(&q);
  cr_assert_eq(ret, 0);

  ret = queue_destroy(&p->queue);
  cr_assert_eq(ret, 0, "Failed to destroy queue");

  ret = pthread_barrier_destroy(&barrier);
  cr_assert_eq(ret, 0, "Failed to destroy barrier");
}
#endif // _POSIX_BARRIERS

Test(queue, init_destroy, .init = init_memory) {
  int ret;
  struct CQueue q;

  ret = queue_init(&q, 1024, &memory::heap);
  cr_assert_eq(ret, 0); // Should succeed

  ret = queue_destroy(&q);
  cr_assert_eq(ret, 0); // Should succeed
}
