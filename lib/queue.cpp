/* Lock-free Multiple-Producer Multiple-consumer (MPMC) queue.
 *
 * Based on Dmitry Vyukov#s Bounded MPMC queue:
 *   http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modiffication, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <villas/log.hpp>
#include <villas/node/memory.hpp>
#include <villas/queue.h>
#include <villas/utils.hpp>

using namespace villas;

// Initialize MPMC queue
int villas::node::queue_init(struct CQueue *q, size_t size,
                             struct memory::Type *m) {
  // Queue size must be 2 exponent
  if (!IS_POW2(size)) {
    size_t old_size = size;
    size = LOG2_CEIL(size);

    auto logger = logging.get("queue");
    logger->warn("A queue size was changed from {} to {}", old_size, size);
  }

  q->buffer_mask = size - 1;
  struct CQueue_cell *buffer =
      (struct CQueue_cell *)memory::alloc(sizeof(struct CQueue_cell) * size, m);
  if (!buffer)
    return -2;

  q->buffer_off = (char *)buffer - (char *)q;

  for (size_t i = 0; i != size; i += 1)
    std::atomic_store_explicit(&buffer[i].sequence, i,
                               std::memory_order_relaxed);

#ifndef __arm__
  std::atomic_store_explicit(&q->tail, 0ul, std::memory_order_relaxed);
  std::atomic_store_explicit(&q->head, 0ul, std::memory_order_relaxed);
#else
  std::atomic_store_explicit(&q->tail, 0u, std::memory_order_relaxed);
  std::atomic_store_explicit(&q->head, 0u, std::memory_order_relaxed);
#endif

  q->state = State::INITIALIZED;

  return 0;
}

int villas::node::queue_destroy(struct CQueue *q) {
  void *buffer = (char *)q + q->buffer_off;
  int ret = 0;

  if (q->state == State::DESTROYED)
    return 0;

  ret = memory::free(buffer);
  if (ret == 0)
    q->state = State::DESTROYED;

  return ret;
}

size_t villas::node::queue_available(struct CQueue *q) {
  return std::atomic_load_explicit(&q->tail, std::memory_order_relaxed) -
         std::atomic_load_explicit(&q->head, std::memory_order_relaxed);
}

int villas::node::queue_push(struct CQueue *q, void *ptr) {
  struct CQueue_cell *cell, *buffer;
  size_t pos, seq;
  intptr_t diff;

  if (std::atomic_load_explicit(&q->state, std::memory_order_relaxed) ==
      State::STOPPED)
    return -1;

  buffer = (struct CQueue_cell *)((char *)q + q->buffer_off);
  pos = std::atomic_load_explicit(&q->tail, std::memory_order_relaxed);
  while (true) {
    cell = &buffer[pos & q->buffer_mask];
    seq = std::atomic_load_explicit(&cell->sequence, std::memory_order_acquire);
    diff = (intptr_t)seq - (intptr_t)pos;

    if (diff == 0) {
      if (std::atomic_compare_exchange_weak_explicit(&q->tail, &pos, pos + 1,
                                                     std::memory_order_relaxed,
                                                     std::memory_order_relaxed))
        break;
    } else if (diff < 0)
      return 0;
    else
      pos = std::atomic_load_explicit(&q->tail, std::memory_order_relaxed);
  }

  cell->data_off = (char *)ptr - (char *)q;
  std::atomic_store_explicit(&cell->sequence, pos + 1,
                             std::memory_order_release);

  return 1;
}

int villas::node::queue_pull(struct CQueue *q, void **ptr) {
  struct CQueue_cell *cell, *buffer;
  size_t pos, seq;
  intptr_t diff;

  if (std::atomic_load_explicit(&q->state, std::memory_order_relaxed) ==
      State::STOPPED)
    return -1;

  buffer = (struct CQueue_cell *)((char *)q + q->buffer_off);
  pos = std::atomic_load_explicit(&q->head, std::memory_order_relaxed);
  while (true) {
    cell = &buffer[pos & q->buffer_mask];
    seq = std::atomic_load_explicit(&cell->sequence, std::memory_order_acquire);
    diff = (intptr_t)seq - (intptr_t)(pos + 1);

    if (diff == 0) {
      if (atomic_compare_exchange_weak_explicit(&q->head, &pos, pos + 1,
                                                std::memory_order_relaxed,
                                                std::memory_order_relaxed))
        break;
    } else if (diff < 0)
      return 0;
    else
      pos = std::atomic_load_explicit(&q->head, std::memory_order_relaxed);
  }

  *ptr = (char *)q + cell->data_off;
  std::atomic_store_explicit(&cell->sequence, pos + q->buffer_mask + 1,
                             std::memory_order_release);

  return 1;
}

int villas::node::queue_push_many(struct CQueue *q, void *ptr[], size_t cnt) {
  int ret;
  size_t i;

  for (ret = 0, i = 0; i < cnt; i++) {
    ret = queue_push(q, ptr[i]);
    if (ret <= 0)
      break;
  }

  if (ret == -1 && i == 0)
    return -1;

  return i;
}

int villas::node::queue_pull_many(struct CQueue *q, void *ptr[], size_t cnt) {
  int ret;
  size_t i;

  for (ret = 0, i = 0; i < cnt; i++) {
    ret = queue_pull(q, &ptr[i]);
    if (ret <= 0)
      break;
  }

  if (ret == -1 && i == 0)
    return -1;

  return i;
}

int villas::node::queue_close(struct CQueue *q) {
  enum State expected = State::INITIALIZED;
  if (std::atomic_compare_exchange_weak_explicit(
          &q->state, &expected, State::STOPPED, std::memory_order_relaxed,
          std::memory_order_relaxed))
    return 0;

  return -1;
}
