/* Infiniband memory allocator.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <infiniband/verbs.h>

#include <villas/exceptions.hpp>
#include <villas/memory/ib.h>
#include <villas/node/memory.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/infiniband.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::utils;
using namespace villas::node;
using namespace villas::node::memory;

struct ibv_mr *villas::node::memory::ib_get_mr(void *ptr) {
  auto *ma = get_allocation(ptr);

  return ma->ib.mr;
}

static struct Allocation *ib_alloc(size_t len, size_t alignment,
                                   struct Type *m) {
  auto *mi = (struct IB *)m->_vd;

  auto *ma = new struct Allocation;
  if (!ma)
    throw MemoryAllocationError();

  ma->type = m;
  ma->length = len;
  ma->alignment = alignment;

  ma->parent =
      mi->parent->alloc(len + sizeof(struct ibv_mr *), alignment, mi->parent);
  ma->address = ma->parent->address;

  if (!mi->pd) {
    auto logger = Log::get("memory:ib");
    logger->error("Protection domain is not registered!");
  }

  ma->ib.mr = ibv_reg_mr(mi->pd, ma->address, ma->length,
                         IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  if (!ma->ib.mr) {
    mi->parent->free(ma->parent, mi->parent);
    delete ma;
    return nullptr;
  }

  return ma;
}

static int ib_free(struct Allocation *ma, struct Type *m) {
  int ret;
  auto *mi = (struct IB *)m->_vd;

  ibv_dereg_mr(ma->ib.mr);

  ret = mi->parent->free(ma->parent, mi->parent);
  if (ret)
    return ret;

  return 0;
}

struct Type *villas::node::memory::ib(NodeCompat *n, struct Type *parent) {
  auto *i = n->getData<struct infiniband>();
  auto *mt = (struct Type *)malloc(sizeof(struct Type));

  mt->name = "ib";
  mt->flags = 0;
  mt->alloc = ib_alloc;
  mt->free = ib_free;
  mt->alignment = 1;

  mt->_vd = malloc(sizeof(struct IB));

  auto *mi = (struct memory::IB *)mt->_vd;

  mi->pd = i->ctx.pd;
  mi->parent = parent;

  return mt;
}
