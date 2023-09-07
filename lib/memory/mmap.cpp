/* mmap memory allocator.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cerrno>
#include <cstdlib>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>

// Required to allocate hugepages on Apple OS X
#ifdef __MACH__
#include <mach/vm_statistics.h>
#endif // __MACH__

#include <villas/exceptions.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/log.hpp>
#include <villas/node/memory.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::memory;

static size_t pgsz = -1;

static size_t hugepgsz = -1;

static Logger logger;

int villas::node::memory::mmap_init(int hugepages) {
  logger = logging.get("memory:mmap");

  pgsz = kernel::getPageSize();
  if (pgsz < 0)
    return -1;

  if (hugepages == 0) {
    logger->warn("Hugepage allocator disabled.");

    default_type = &mmap;
    return 0;
  }

  if (!utils::isPrivileged()) {
    logger->warn(
        "Running in an unprivileged environment. Hugepages are not used!");

    default_type = &mmap;
    return 0;
  }

  hugepgsz = kernel::getHugePageSize();
  if (hugepgsz < 0) {
    logger->warn("Failed to determine hugepage size.");

    return -1;
  }

#if defined(__linux__) && defined(__x86_64__)
  int ret, pagecnt;

  pagecnt = kernel::getNrHugepages();
  if (pagecnt < hugepages) {
    ret = kernel::setNrHugepages(hugepages);
    if (ret) {
      logger->warn("Failed to reserved hugepages. Please reserve manually by "
                   "running as root:");
      logger->warn("   $ echo {} > /proc/sys/vm/nr_hugepages", hugepages);

      return -1;
    }

    logger->debug("Increased number of reserved hugepages from {} to {}",
                  pagecnt, hugepages);
  }

  default_type = &mmap_hugetlb;
#else
  logger->debug("Hugepages not supported on this system. Falling back to "
                "standard mmap() allocator.");

  default_type = &mmap;
#endif

  return 0;
}

// Allocate memory backed by mmaps with malloc() like interface
static struct Allocation *mmap_alloc(size_t len, size_t alignment,
                                     struct Type *m) {
  int flags, fd;
  size_t sz;

  auto *ma = new struct Allocation;
  if (!ma)
    throw MemoryAllocationError();

  if (m->flags & (int)Flags::HUGEPAGE) {
#ifdef __linux__
    flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB;
#else
    flags = MAP_PRIVATE | MAP_ANONYMOUS;
#endif

#ifdef __MACH__
    fd = VM_FLAGS_SUPERPAGE_SIZE_2MB;
#else
    fd = -1;
#endif
    sz = hugepgsz;
  } else {
    flags = MAP_PRIVATE | MAP_ANONYMOUS;
    fd = -1;

    sz = pgsz;
  }

  /* We must make sure that len is a multiple of the page size
	 *
	 * See: https://lkml.org/lkml/2014/10/22/925
	 */
  ma->length = ALIGN(len, sz);
  ma->alignment = ALIGN(alignment, sz);
  ma->type = m;

  ma->address =
      ::mmap(nullptr, ma->length, PROT_READ | PROT_WRITE, flags, fd, 0);
  if (ma->address == MAP_FAILED) {
    delete ma;
    return nullptr;
  }

  return ma;
}

static int mmap_free(struct Allocation *ma, struct Type *m) {
  int ret;

  ret = munmap(ma->address, ma->length);
  if (ret)
    return ret;

  return 0;
}

struct Type memory::mmap = {.name = "mmap",
                            .flags = (int)Flags::MMAP,
                            .alignment = 12, // 4k page
                            .alloc = mmap_alloc,
                            .free = mmap_free};

struct Type memory::mmap_hugetlb = {.name = "mmap_hugetlb",
                                    .flags =
                                        (int)Flags::MMAP | (int)Flags::HUGEPAGE,
                                    .alignment = 21, // 2 MiB hugepage
                                    .alloc = mmap_alloc,
                                    .free = mmap_free};
