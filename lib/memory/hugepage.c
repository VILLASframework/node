/** Hugepage memory allocator.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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
#elif defined(__linux__)
  #include <villas/kernel/kernel.h>
#endif

#include <villas/log.h>
#include <villas/memory_type.h>
#include <villas/utils.h>

#define HUGEPAGESIZE	(1 << 21) /* 2 MiB */

/** Allocate memory backed by hugepages with malloc() like interface */
static void * memory_hugepage_alloc(struct memory_type *m, size_t len, size_t alignment)
{
	void *ret;
	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;

#ifdef __MACH__
	flags |= VM_FLAGS_SUPERPAGE_SIZE_2MB;
#elif defined(__linux__)
	flags |= MAP_HUGETLB;

	if (getuid() == 0)
		flags |= MAP_LOCKED;
#endif

	ret = mmap(NULL, len, prot, flags, -1, 0);
	if (ret == MAP_FAILED)
		return NULL;

	return ret;
}

static int memory_hugepage_free(struct memory_type *m, void *ptr, size_t len)
{
	/** We must make sure that len is a multiple of the hugepage size
	 *
	 * See: https://lkml.org/lkml/2014/10/22/925
	 */
	len = ALIGN(len, HUGEPAGESIZE);

	return munmap(ptr, len);
}

struct memory_type memory_hugepage = {
	.name = "mmap_hugepages",
	.flags = MEMORY_MMAP | MEMORY_HUGEPAGE,
	.alloc = memory_hugepage_alloc,
	.free = memory_hugepage_free,
	.alignment = 21 /* 2 MiB hugepage */
};
