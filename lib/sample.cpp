/* The internal datastructure for a sample of simulation data.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cinttypes>
#include <cmath>
#include <cstring>

#include <villas/colors.hpp>
#include <villas/exceptions.hpp>
#include <villas/list.hpp>
#include <villas/pool.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>
#include <villas/timing.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int villas::node::sample_init(struct Sample *s) {
  struct Pool *p = sample_pool(s);

  s->length = 0;
  s->capacity = (p->blocksz - sizeof(struct Sample)) / sizeof(s->data[0]);
  s->refcnt = ATOMIC_VAR_INIT(1);

  new (&s->signals) std::shared_ptr<SignalList>;

  return 0;
}

struct Sample *villas::node::sample_alloc(struct Pool *p) {
  struct Sample *s;

  s = (struct Sample *)pool_get(p);
  if (!s)
    return nullptr;

  s->pool_off = (char *)p - (char *)s;

  int ret = sample_init(s);
  if (ret) {
    pool_put(p, s);
    return nullptr;
  }

  return s;
}

struct Sample *villas::node::sample_alloc_mem(int capacity) {
  size_t sz = SAMPLE_LENGTH(capacity);

  auto *s = (struct Sample *)new char[sz];
  if (!s)
    throw MemoryAllocationError();

  memset((void *)s, 0, sz);

  s->pool_off = SAMPLE_NON_POOL;

  s->length = 0;
  s->capacity = capacity;
  s->refcnt = ATOMIC_VAR_INIT(1);

  return s;
}

void villas::node::sample_free(struct Sample *s) {
  struct Pool *p = sample_pool(s);

  if (p)
    pool_put(p, s);
  else
    delete[](char *) s;
}

int villas::node::sample_alloc_many(struct Pool *p, struct Sample *smps[],
                                    int cnt) {
  int ret;

  ret = pool_get_many(p, (void **)smps, cnt);
  if (ret < 0)
    return ret;

  for (int i = 0; i < ret; i++) {
    smps[i]->pool_off = (char *)p - (char *)smps[i];

    sample_init(smps[i]);
  }

  return ret;
}

void villas::node::sample_free_many(struct Sample *smps[], int cnt) {
  for (int i = 0; i < cnt; i++)
    sample_free(smps[i]);
}

int villas::node::sample_decref_many(struct Sample *const smps[], int cnt) {
  int released = 0;

  for (int i = 0; i < cnt; i++) {
    if (sample_decref(smps[i]) == 0)
      released++;
  }

  return released;
}

int villas::node::sample_incref_many(struct Sample *const smps[], int cnt) {
  for (int i = 0; i < cnt; i++)
    sample_incref(smps[i]);

  return cnt;
}

int villas::node::sample_incref(struct Sample *s) {
  return atomic_fetch_add(&s->refcnt, 1) + 1;
}

int villas::node::sample_decref(struct Sample *s) {
  int prev = atomic_fetch_sub(&s->refcnt, 1);

  // Did we had the last reference?
  if (prev == 1)
    sample_free(s);

  return prev - 1;
}

int villas::node::sample_copy(struct Sample *dst, const struct Sample *src) {
  dst->length = MIN(src->length, dst->capacity);

  dst->sequence = src->sequence;
  dst->flags = src->flags;
  dst->ts = src->ts;
  dst->signals = src->signals;

  memcpy(&dst->data, &src->data, SAMPLE_DATA_LENGTH(dst->length));

  return 0;
}

struct Sample *villas::node::sample_clone(struct Sample *orig) {
  struct Sample *clone;
  struct Pool *pool;

  pool = sample_pool(orig);
  if (!pool)
    return nullptr;

  clone = sample_alloc(pool);
  if (!clone)
    return nullptr;

  sample_copy(clone, orig);

  return clone;
}

int villas::node::sample_clone_many(struct Sample *dsts[],
                                    const struct Sample *const srcs[],
                                    int cnt) {
  int alloced, copied;
  struct Pool *pool;

  if (cnt <= 0)
    return 0;

  pool = sample_pool(srcs[0]);
  if (!pool)
    return 0;

  alloced = sample_alloc_many(pool, dsts, cnt);

  copied = sample_copy_many(dsts, srcs, alloced);

  return copied;
}

int villas::node::sample_copy_many(struct Sample *const dsts[],
                                   const struct Sample *const srcs[], int cnt) {
  for (int i = 0; i < cnt; i++)
    sample_copy(dsts[i], srcs[i]);

  return cnt;
}

int villas::node::sample_cmp(struct Sample *a, struct Sample *b, double epsilon,
                             int flags) {
  if ((a->flags & b->flags & flags) != flags) {
    printf("flags: a=%#x, b=%#x, wanted=%#x\n", a->flags, b->flags, flags);
    return -1;
  }

  // Compare sequence no
  if (flags & (int)SampleFlags::HAS_SEQUENCE) {
    if (a->sequence != b->sequence) {
      printf("sequence no: %" PRIu64 " != %" PRIu64 "\n", a->sequence,
             b->sequence);
      return 2;
    }
  }

  // Compare timestamp
  if (flags & (int)SampleFlags::HAS_TS_ORIGIN) {
    if (time_delta(&a->ts.origin, &b->ts.origin) > epsilon) {
      printf("ts.origin: %f != %f\n", time_to_double(&a->ts.origin),
             time_to_double(&b->ts.origin));
      return 3;
    }
  }

  // Compare data
  if (flags & (int)SampleFlags::HAS_DATA) {
    if (a->length != b->length) {
      printf("length: %u != %u\n", a->length, b->length);
      return 4;
    }

    for (unsigned i = 0; i < a->length; i++) {
      // Compare format
      if (sample_format(a, i) != sample_format(b, i))
        return 6;

      switch (sample_format(a, i)) {
      case SignalType::FLOAT:
        if (fabs(a->data[i].f - b->data[i].f) > epsilon) {
          printf("data[%u].f: %f != %f\n", i, a->data[i].f, b->data[i].f);
          return 5;
        }
        break;

      case SignalType::INTEGER:
        if (a->data[i].i != b->data[i].i) {
          printf("data[%u].i: %" PRId64 " != %" PRId64 "\n", i, a->data[i].i,
                 b->data[i].i);
          return 5;
        }
        break;

      case SignalType::BOOLEAN:
        if (a->data[i].b != b->data[i].b) {
          printf("data[%u].b: %s != %s\n", i, a->data[i].b ? "true" : "false",
                 b->data[i].b ? "true" : "false");
          return 5;
        }
        break;

      case SignalType::COMPLEX:
        if (std::abs(a->data[i].z - b->data[i].z) > epsilon) {
          printf("data[%u].z: %f+%fi != %f+%fi\n", i, std::real(a->data[i].z),
                 std::imag(a->data[i].z), std::real(b->data[i].z),
                 std::imag(b->data[i].z));
          return 5;
        }
        break;

      default: {
      }
      }
    }
  }

  return 0;
}

enum SignalType villas::node::sample_format(const struct Sample *s,
                                            unsigned idx) {
  auto sig = s->signals->getByIndex(idx);

  return sig ? sig->type : SignalType::INVALID;
}

void villas::node::sample_dump(Logger logger, struct Sample *s) {
  logger->info("Sample: sequence={}, length={}, capacity={}, "
               "flags={:#x}, #signals={}, "
               "refcnt={}, pool_off={:#x}",
               s->sequence, s->length, s->capacity, s->flags,
               s->signals ? s->signals->size() : -1, atomic_load(&s->refcnt),
               s->pool_off);

  if (s->flags & (int)SampleFlags::HAS_TS_ORIGIN)
    logger->info("  ts.origin={}.{:09d}", s->ts.origin.tv_sec,
                 s->ts.origin.tv_nsec);

  if (s->flags & (int)SampleFlags::HAS_TS_RECEIVED)
    logger->info("  ts.received={}.{:09d}", s->ts.received.tv_sec,
                 s->ts.received.tv_nsec);

  if (s->signals) {
    logger->info("  Signals:");
    s->signals->dump(logger, s->data, s->length);
  }
}

void villas::node::sample_data_insert(struct Sample *smp,
                                      const union SignalData *src,
                                      size_t offset, size_t len) {
  memmove(&smp->data[offset + len], &smp->data[offset],
          sizeof(smp->data[0]) * (smp->length - offset));
  memcpy(&smp->data[offset], src, sizeof(smp->data[0]) * len);

  smp->length += len;
}

void villas::node::sample_data_remove(struct Sample *smp, size_t offset,
                                      size_t len) {
  size_t sz = sizeof(smp->data[0]) * len;

  memmove(&smp->data[offset], &smp->data[offset + len], sz);

  smp->length -= len;
}
