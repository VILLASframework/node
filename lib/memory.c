/** Memory allocators.
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

#include <sys/time.h>
#include <sys/resource.h>

#include <villas/log.h>
#include <villas/memory.h>
#include <villas/utils.h>
#include <villas/kernel/kernel.h>

int memory_init(int hugepages)
{
#ifdef __linux__
	int ret, pagecnt, pagesz;
	struct rlimit l;

	info("Initialize memory sub-system");

	pagecnt = kernel_get_nr_hugepages();
	if (pagecnt < hugepages) { INDENT
		kernel_set_nr_hugepages(hugepages);
		debug(LOG_MEM | 2, "Reserved %d hugepages (was %d)", hugepages, pagecnt);
	}

	pagesz = kernel_get_hugepage_size();
	if (pagesz < 0)
		return -1;

	ret = getrlimit(RLIMIT_MEMLOCK, &l);
	if (ret)
		return ret;

	if (l.rlim_cur < pagesz * pagecnt) { INDENT
		l.rlim_cur = pagesz * pagecnt;
		l.rlim_max = l.rlim_cur;

		ret = setrlimit(RLIMIT_MEMLOCK, &l);
		if (ret)
			return ret;

		debug(LOG_MEM | 2, "Increased ressource limit of locked memory to %d bytes", pagesz * pagecnt);
	}
#endif
	return 0;
}

void * memory_alloc(struct memory_type *m, size_t len)
{
	void *ptr = m->alloc(m, len, sizeof(void *));

	debug(LOG_MEM | 5, "Allocated %#zx bytes of %s memory: %p", len, m->name, ptr);

	return ptr;
}

void * memory_alloc_aligned(struct memory_type *m, size_t len, size_t alignment)
{
	void *ptr = m->alloc(m, len, alignment);

	debug(LOG_MEM | 5, "Allocated %#zx bytes of %#zx-byte-aligned %s memory: %p", len, alignment, m->name, ptr);

	return ptr;
}

int memory_free(struct memory_type *m, void *ptr, size_t len)
{
	debug(LOG_MEM | 5, "Releasing %#zx bytes of %s memory", len, m->name);

	return m->free(m, ptr, len);
}
