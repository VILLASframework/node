/* Memory pool for fixed size objects.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/exceptions.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/log.hpp>
#include <villas/node/memory.hpp>
#include <villas/pool.hpp>
#include <villas/utils.hpp>

using namespace villas;

int villas::node::pool_init(struct Pool *p, size_t cnt, size_t blocksz,
                            struct memory::Type *m) {
  int ret;
  auto logger = logging.get("pool");

  // Make sure that we use a block size that is aligned to the size of a cache line
  p->alignment = kernel::getCachelineSize();
  p->blocksz = p->alignment * CEIL(blocksz, p->alignment);
  p->len = cnt * p->blocksz;

  logger->debug("New memory pool: alignment={}, blocksz={}, len={}, memory={}",
                p->alignment, p->blocksz, p->len, m->name);

  void *buffer = memory::alloc_aligned(p->len, p->alignment, m);
  if (!buffer)
    throw MemoryAllocationError();

  logger->debug("Allocated {:#x} bytes for memory pool", p->len);

  p->buffer_off = (char *)buffer - (char *)p;

  ret = queue_init(&p->queue, LOG2_CEIL(cnt), m);
  if (ret)
    return ret;

  for (unsigned i = 0; i < cnt; i++)
    queue_push(&p->queue, (char *)buffer + i * p->blocksz);

  p->state = State::INITIALIZED;

  return 0;
}

int villas::node::pool_destroy(struct Pool *p) {
  int ret;

  if (p->state == State::DESTROYED)
    return 0;

  ret = queue_destroy(&p->queue);
  if (ret)
    return ret;

  void *buffer = (char *)p + p->buffer_off;
  ret = memory::free(buffer);
  if (ret == 0)
    p->state = State::DESTROYED;

  return ret;
}

ssize_t villas::node::pool_get_many(struct Pool *p, void *blocks[],
                                    size_t cnt) {
  return queue_pull_many(&p->queue, blocks, cnt);
}

ssize_t villas::node::pool_put_many(struct Pool *p, void *blocks[],
                                    size_t cnt) {
  return queue_push_many(&p->queue, blocks, cnt);
}

void *villas::node::pool_get(struct Pool *p) {
  void *ptr;
  return queue_pull(&p->queue, &ptr) == 1 ? ptr : nullptr;
}

int villas::node::pool_put(struct Pool *p, void *buf) {
  return queue_push(&p->queue, buf);
}
