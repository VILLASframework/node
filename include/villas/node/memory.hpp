/* Memory allocators.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <villas/node/config.hpp>
#include <villas/node/memory_type.hpp>

#ifdef IBVERBS_FOUND
#include <infiniband/verbs.h>
#endif // IBVERBS_FOUND

namespace villas {
namespace node {
namespace memory {

/* Descriptor of a memory block. Associated block always starts at
 * &m + sizeof(struct Block). */
struct Block {
  struct Block *prev;
  struct Block *next;
  size_t length; // Length of the block; doesn't include the descriptor itself
  bool used;
};

// TODO: Unused for now
struct Allocation {
  struct Type *type;

  struct Allocation *parent;

  void *address;
  size_t alignment;
  size_t length;

  union {
#ifdef IBVERBS_FOUND
    struct {
      struct ibv_mr *mr;
    } ib;
#endif
    struct {
      struct Block *block;
    } managed;
  };
};

// Initialize memory subsystem
int init(int hugepages) __attribute__((warn_unused_result));

int lock(size_t lock);

/* Allocate \p len bytes memory of type \p m.
 *
 * @retval nullptr If allocation failed.
 * @retval <>0  If allocation was successful.
 */
void *alloc(size_t len, struct Type *m = default_type);

void *alloc_aligned(size_t len, size_t alignment,
                    struct Type *m = default_type);

int free(void *ptr);

struct Allocation *get_allocation(void *ptr);

} // namespace memory
} // namespace node
} // namespace villas
