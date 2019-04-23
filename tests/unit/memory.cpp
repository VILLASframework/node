/** Unit tests for memory management
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

#include <criterion/criterion.h>
#include <criterion/theories.h>

#include <errno.h>

#include <villas/memory.h>
#include <villas/utils.hpp>
#include <villas/log.hpp>

extern void init_memory();

#define PAGESIZE (1 << 12)
#define HUGEPAGESIZE (1 << 21)

TheoryDataPoints(memory, aligned) = {
	DataPoints(size_t, 1, 32, 55, 1 << 10, PAGESIZE, HUGEPAGESIZE),
	DataPoints(size_t, 1, 8, PAGESIZE, PAGESIZE),
	DataPoints(enum memory_type_flags, MEMORY_HEAP, MEMORY_HUGEPAGE, MEMORY_HUGEPAGE)
};

Theory((size_t len, size_t align, enum memory_type_flags memory_type), memory, aligned, .init = init_memory) {
	int ret;
	void *ptr;

	struct memory_type *mt = memory_type_lookup(memory_type);

	ptr = memory_alloc_aligned(mt, len, align);
	cr_assert_not_null(ptr, "Failed to allocate memory");

	cr_assert(IS_ALIGNED(ptr, align), "Memory at %p is not alligned to %#zx byte bounary", ptr, align);

#ifndef __APPLE__
	if (mt == &memory_hugepage) {
		cr_assert(IS_ALIGNED(ptr, HUGEPAGESIZE), "Memory at %p is not alligned to %#x byte bounary", ptr, HUGEPAGESIZE);
	}
#endif

	ret = memory_free(ptr);
	cr_assert_eq(ret, 0, "Failed to release memory: ret=%d, ptr=%p, len=%zu: %s", ret, ptr, len, strerror(errno));
}

Test(memory, manager, .init = init_memory) {
	size_t total_size;
	size_t max_block;

	int ret;
	void *p, *p1, *p2, *p3;
	struct memory_type *m;

	total_size = 1 << 10;
	max_block = total_size - sizeof(struct memory_type) - sizeof(struct memory_block);

	p = memory_alloc(&memory_heap, total_size);
	cr_assert_not_null(p);

	m = memory_managed(p, total_size);
	cr_assert_not_null(m);

	p1 = memory_alloc(m, 16);
	cr_assert_not_null(p1);

	p2 = memory_alloc(m, 32);
	cr_assert_not_null(p2);

	ret = memory_free(p1);
	cr_assert(ret == 0);

	p1 = memory_alloc_aligned(m, 128, 128);
	cr_assert_not_null(p1);
	cr_assert(IS_ALIGNED(p1, 128));

	p3 = memory_alloc_aligned(m, 128, 256);
	cr_assert(p3);
	cr_assert(IS_ALIGNED(p3, 256));

	ret = memory_free(p2);
	cr_assert(ret == 0);

	ret = memory_free(p1);
	cr_assert(ret == 0);

	ret = memory_free(p3);
	cr_assert(ret == 0);

	p1 = memory_alloc(m, max_block);
	cr_assert_not_null(p1);

	ret = memory_free(p1);
	cr_assert(ret == 0);

	ret = memory_free(p);
	cr_assert(ret == 0);
}
