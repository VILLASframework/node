/** mmap memory allocator.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstdlib>
#include <cerrno>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

/* Required to allocate hugepages on Apple OS X */
#ifdef __MACH__
  #include <mach/vm_statistics.h>
#endif /* __MACH__ */

#include <villas/kernel/kernel.hpp>
#include <villas/memory.h>
#include <villas/utils.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/exceptions.hpp>
#include <villas/log.hpp>

using namespace villas;
using namespace villas;
using namespace villas::utils;

static size_t pgsz = -1;
static size_t hugepgsz = -1;
static Logger logger;

int memory_mmap_init(int hugepages)
{
	logger = logging.get("memory:mmap");

	pgsz = kernel::getPageSize();
	if (pgsz < 0)
		return -1;

	if (hugepages > 0) {
		hugepgsz = kernel::getHugePageSize();
		if (hugepgsz < 0) {
			logger->warn("Failed to determine hugepage size.");

			memory_default = &memory_mmap;
			return 0;
		}

#if defined(__linux__) && defined(__x86_64__)
		int ret, pagecnt;

		pagecnt = kernel::getNrHugepages();
		if (pagecnt < hugepages) {
			if (getuid() == 0) {
				ret = kernel::setNrHugepages(hugepages);
				if (ret) {
					logger->warn("Failed to increase number of reserved hugepages. Falling back to standard mmap() allocator.");

					if (isContainer()) {
						logger->warn("Please run the container in the privileged mode:");
						logger->warn("    $ docker run --privileged ...");
					}

					memory_default = &memory_mmap;
				}
				else {
					logger->debug("Increased number of reserved hugepages from {} to {}", pagecnt, hugepages);
					memory_default = &memory_mmap_hugetlb;
				}
			}
			else {
				logger->warn("Failed to reserved hugepages. Please reserve manually by running as root:");
				logger->warn("   $ echo {} > /proc/sys/vm/nr_hugepages", hugepages);
				memory_default = &memory_mmap;
			}
		}
		else
			memory_default = &memory_mmap_hugetlb;
#else
		memory_default = &memory_mmap;
#endif
	}
	else {
		logger->warn("Hugepage allocator disabled.");
		memory_default = &memory_mmap;
	}

	return 0;
}

/** Allocate memory backed by mmaps with malloc() like interface */
static struct memory_allocation * memory_mmap_alloc(size_t len, size_t alignment, struct memory_type *m)
{
	int flags, fd;
	size_t sz;

	auto *ma = new struct memory_allocation;
	if (!ma)
		throw MemoryAllocationError();

	if (m->flags & (int) MemoryFlags::HUGEPAGE) {
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
	}
	else {
		flags = MAP_PRIVATE | MAP_ANONYMOUS;
		fd = -1;

		sz = pgsz;
	}

	/** We must make sure that len is a multiple of the page size
	 *
	 * See: https://lkml.org/lkml/2014/10/22/925
	 */
	ma->length = ALIGN(len, sz);
	ma->alignment = ALIGN(alignment, sz);
	ma->type = m;

	ma->address = mmap(nullptr, ma->length, PROT_READ | PROT_WRITE, flags, fd, 0);
	if (ma->address == MAP_FAILED) {
		delete ma;
		return nullptr;
	}

	return ma;
}

static int memory_mmap_free(struct memory_allocation *ma, struct memory_type *m)
{
	int ret;

	ret = munmap(ma->address, ma->length);
	if (ret)
		return ret;

	return 0;
}

struct memory_type memory_mmap = {
	.name = "mmap",
	.flags = (int) MemoryFlags::MMAP,
	.alignment = 12, /* 4k page */
	.alloc = memory_mmap_alloc,
	.free = memory_mmap_free
};

struct memory_type memory_mmap_hugetlb = {
	.name = "mmap_hugetlb",
	.flags = (int) MemoryFlags::MMAP | (int) MemoryFlags::HUGEPAGE,
	.alignment = 21, /* 2 MiB hugepage */
	.alloc = memory_mmap_alloc,
	.free = memory_mmap_free
};
