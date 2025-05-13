/* Memory allocators.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#include <malloc.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include <villas/exceptions.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/log.hpp>
#include <villas/node/memory.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::memory;

static std::unordered_map<void *, struct Allocation *> allocations;
static Logger logger;

int villas::node::memory::init(int hugepages) {
  int ret;

  logger = Log::get("memory");

  logger->info("Initialize memory sub-system: #hugepages={}", hugepages);

  ret = mmap_init(hugepages);
  if (ret < 0)
    return ret;

  size_t lock_sz = kernel::getHugePageSize() * hugepages;

  ret = lock(lock_sz);
  if (ret)
    return ret;

  return 0;
}

int villas::node::memory::lock(size_t sz) {
  int ret;

  if (!utils::isPrivileged()) {
    logger->warn(
        "Running in an unprivileged environment. Memory is not locked to RAM!");
    return 0;
  }

#ifdef __linux__
#ifndef __arm__
  struct rlimit l;

  // Increase ressource limit for locked memory
  ret = getrlimit(RLIMIT_MEMLOCK, &l);
  if (ret)
    return ret;

  if (l.rlim_cur < sz) {
    if (l.rlim_max < sz) {
      if (getuid() != 0) {
        logger->warn("Failed to increase ressource limit of locked memory. "
                     "Please increase manually by running as root:");
        logger->warn("   $ ulimit -Hl {}", sz);

        return 0;
      }

      l.rlim_max = sz;
    }

    l.rlim_cur = sz;

    ret = setrlimit(RLIMIT_MEMLOCK, &l);
    if (ret)
      return ret;

    logger->debug("Increased ressource limit of locked memory to {} bytes", sz);
  }

#endif /* __arm__ */

  // Disable usage of mmap() for malloc()
  mallopt(M_MMAP_MAX, 0);
  mallopt(M_TRIM_THRESHOLD, -1);

#ifdef _POSIX_MEMLOCK
  // Lock all current and future memory allocations
  ret = mlockall(MCL_CURRENT | MCL_FUTURE);
  if (ret)
    return -1;
#endif // _POSIX_MEMLOCK
#endif // __linux__

  return 0;
}

void *villas::node::memory::alloc(size_t len, struct Type *m) {
  return alloc_aligned(len, sizeof(void *), m);
}

void *villas::node::memory::alloc_aligned(size_t len, size_t alignment,
                                          struct Type *m) {
  struct Allocation *ma = m->alloc(len, alignment, m);
  if (ma == nullptr) {
    logger->warn("Memory allocation of type {} failed. reason={}", m->name,
                 strerror(errno));
    return nullptr;
  }

  allocations[ma->address] = ma;

  logger->debug("Allocated {:#x} bytes of {:#x}-byte-aligned {} memory: {}",
                ma->length, ma->alignment, ma->type->name, ma->address);

  return ma->address;
}

int villas::node::memory::free(void *ptr) {
  int ret;

  // Find corresponding memory allocation entry
  struct Allocation *ma = allocations[ptr];
  if (!ma)
    return -1;

  logger->debug("Releasing {:#x} bytes of {} memory: {}", ma->length,
                ma->type->name, ma->address);

  ret = ma->type->free(ma, ma->type);
  if (ret)
    return ret;

  // Remove allocation entry
  auto iter = allocations.find(ptr);
  if (iter == allocations.end())
    return -1;

  allocations.erase(iter);
  delete ma;

  return 0;
}

struct Allocation *villas::node::memory::get_allocation(void *ptr) {
  return allocations[ptr];
}

void villas::node::memory::prefault_heap(size_t sz) {
  auto pgsz = kernel::getPageSize();

  char *dummy = new char[sz];
  if (!dummy)
    throw MemoryAllocationError();

  for (size_t i = 0; i < sz; i += pgsz)
    dummy[i] = 1;

  delete[] dummy;
}

void villas::node::memory::prefault_stack() {
  auto pgsz = kernel::getPageSize();

  unsigned char dummy[MAX_SAFE_STACK] __attribute__((unused));
  for (size_t i = 0; i < MAX_SAFE_STACK; i += pgsz)
    dummy[i] = 1;
}

struct Type *villas::node::memory::default_type = nullptr;
