/* Unit tests for queue_signalled.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <poll.h>
#include <pthread.h>

#include <villas/node/memory.hpp>
#include <villas/queue_signalled.h>
#include <villas/utils.hpp>

using namespace villas::node;

extern void init_memory();

#define NUM_ELEM 1000

struct param {
  enum QueueSignalledMode mode;
  int flags;
  bool polled;
};

static void *producer(void *ctx) {
  int ret;
  struct CQueueSignalled *q = (struct CQueueSignalled *)ctx;

  for (intptr_t i = 0; i < NUM_ELEM; i++) {
    ret = queue_signalled_push(q, (void *)i);
    if (ret != 1)
      return (void *)1; // Indicates an error to the parent thread

    usleep(0.1e-3 * 1e6); // 1 ms
  }

  return nullptr;
}

static void *consumer(void *ctx) {
  int ret;
  struct CQueueSignalled *q = (struct CQueueSignalled *)ctx;

  void *data[NUM_ELEM];

  for (intptr_t i = 0; i < NUM_ELEM;) {
    ret = queue_signalled_pull_many(q, data, ARRAY_LEN(data));
    if (ret <= 0)
      return (void *)1; // Indicates an error to the parent thread

    for (intptr_t j = 0; j < ret; j++, i++) {
      if ((intptr_t)data[j] != i)
        return (void *)2; // Indicates an error to the parent thread
    }
  }

  return nullptr;
}

void *polled_consumer(void *ctx) {
  int ret, fd;
  struct CQueueSignalled *q = (struct CQueueSignalled *)ctx;

  fd = queue_signalled_fd(q);
  cr_assert_geq(fd, 0);

  struct pollfd pfd = {.fd = fd, .events = POLLIN};

  for (intptr_t i = 0; i < NUM_ELEM; i++) {
  again:
    ret = poll(&pfd, 1, -1);
    if (ret < 0)
      return (void *)3;
    else if (ret == 0)
      goto again;

    void *p;
    ret = queue_signalled_pull(q, &p);
    if (ret != 1)
      return (void *)1; // Indicates an error to the parent thread

    if ((intptr_t)p != i)
      return (void *)2; // Indicates an error to the parent thread
  }

  return nullptr;
}

ParameterizedTestParameters(queue_signalled, simple) {
  static struct param params[] = {
    {QueueSignalledMode::AUTO, 0, false},
    {QueueSignalledMode::PTHREAD, 0, false},
    {QueueSignalledMode::PTHREAD, 0, false},
    {QueueSignalledMode::PTHREAD, (int)QueueSignalledFlags::PROCESS_SHARED,
     false},
    {QueueSignalledMode::POLLING, 0, false},
#if defined(__linux__) && defined(HAS_EVENTFD)
    {QueueSignalledMode::EVENTFD, 0, false},
    {QueueSignalledMode::EVENTFD, 0, true}
#endif
  };

  return cr_make_param_array(struct param, params, ARRAY_LEN(params));
}

// cppcheck-suppress unknownMacro
ParameterizedTest(struct param *param, queue_signalled, simple, .timeout = 5,
                  .init = init_memory) {
  int ret;
  void *r1, *r2;
  struct CQueueSignalled q;

  pthread_t t1, t2;

  ret = queue_signalled_init(&q, LOG2_CEIL(NUM_ELEM), &memory::heap,
                             param->mode, param->flags);
  cr_assert_eq(ret, 0, "Failed to initialize queue: mode=%d, flags=%#x, ret=%d",
               (int)param->mode, param->flags, ret);

  ret = pthread_create(&t1, nullptr, producer, &q);
  cr_assert_eq(ret, 0);

  ret = pthread_create(&t2, nullptr, param->polled ? polled_consumer : consumer,
                       &q);
  cr_assert_eq(ret, 0);

  ret = pthread_join(t1, &r1);
  cr_assert_eq(ret, 0);

  ret = pthread_join(t2, &r2);
  cr_assert_eq(ret, 0);

  cr_assert_null(r1, "Producer failed: %p", r1);
  cr_assert_null(r2, "Consumer failed: %p", r2);

  ret = queue_signalled_available(&q);
  cr_assert_eq(ret, 0);

  ret = queue_signalled_close(&q);
  cr_assert_eq(ret, 0);

  ret = queue_signalled_destroy(&q);
  cr_assert_eq(ret, 0);
}
