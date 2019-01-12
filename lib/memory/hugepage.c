/** Hugepage memory allocator.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

/* Required to allocate hugepages on Apple OS X */
#ifdef __MACH__
  #include <mach/vm_statistics.h>
#endif

#include <villas/kernel/kernel.h>
#include <villas/log.h>
#include <villas/memory.h>
#include <villas/utils.h>
#include <villas/kernel/kernel.h>

static size_t pgsz = -1;
static size_t hugepgsz = -1;

int memory_hugepage_init(int hugepages)
{
	pgsz = kernel_get_page_size();
	if (pgsz < 0)
		return -1;

	hugepgsz = kernel_get_hugepage_size();
	if (hugepgsz < 0)
		return -1;

#if defined(__linux__) && defined(__x86_64__)
	int pagecnt;

	pagecnt = kernel_get_nr_hugepages();
	if (pagecnt < hugepages) {
		if (getuid() == 0) {
			kernel_set_nr_hugepages(hugepages);
			debug(LOG_MEM | 2, "Increased number of reserved hugepages from %d to %d", pagecnt, hugepages);
		}
		else {
			warning("Failed to reserved hugepages. Please re-run as super-user or reserve manually via:");
			warning("   $ echo %d > /proc/sys/vm/nr_hugepages", hugepages);

			return -1;
		}
	}
#endif

	return 0;
}

/** Allocate memory backed by hugepages with malloc() like interface */
static struct memory_allocation * memory_hugepage_alloc(struct memory_type *m, size_t len, size_t alignment)
{
	static bool use_huge = true;

	int ret, flags, fd;
	size_t sz;

	struct memory_allocation *ma = alloc(sizeof(struct memory_allocation));
	if (!ma)
		return NULL;

retry:	if (use_huge) {
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

	/** We must make sure that len is a multiple of the (huge)page size
	 *
	 * See: https://lkml.org/lkml/2014/10/22/925
	 */
	ma->length = ALIGN(len, sz);
	ma->alignment = ALIGN(alignment, sz);
	ma->type = m;

	ma->address = mmap(NULL, ma->length, PROT_READ | PROT_WRITE, flags, fd, 0);
	if (ma->address == MAP_FAILED) {
		if (use_huge) {
			warning("Failed to map hugepages, try with normal pages instead");
			use_huge = false;
			goto retry;
		}
		else {
			free(ma);
			return NULL;
		}
	}

	if (getuid() == 0) {
		ret = mlock(ma->address, ma->length);
		if (ret)
			return NULL;
	}

	return ma;
}

static int memory_hugepage_free(struct memory_type *m, struct memory_allocation *ma)
{
	int ret;

	ret = munmap(ma->address, ma->length);
	if (ret)
		return ret;

	return 0;
}

struct memory_type memory_hugepage = {
	.name = "mmap_hugepages",
	.flags = MEMORY_MMAP | MEMORY_HUGEPAGE,
	.alloc = memory_hugepage_alloc,
	.free = memory_hugepage_free,
	.alignment = 21 /* 2 MiB hugepage */
};
