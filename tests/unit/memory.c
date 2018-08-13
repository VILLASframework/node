/** Unit tests for memory management
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

#include <criterion/criterion.h>
#include <criterion/theories.h>

#include <errno.h>

#include <villas/memory.h>
#include <villas/utils.h>

#define HUGEPAGESIZE (1 << 22)

TheoryDataPoints(memory, aligned) = {
	DataPoints(size_t, 1, 32, 55, 1 << 10, 1 << 20),
	DataPoints(size_t, 1, 8, 1 << 12),
	DataPoints(struct memory_type *, &memory_type_heap, &memory_hugepage)
};

Theory((size_t len, size_t align, struct memory_type *m), memory, aligned) {
	int ret;
	void *ptr;

	ret = memory_init(100);
	cr_assert(!ret);

	ptr = memory_alloc_aligned(m, len, align);
	cr_assert_neq(ptr, NULL, "Failed to allocate memory");

	cr_assert(IS_ALIGNED(ptr, align));

	if (m == &memory_hugepage) {
		cr_assert(IS_ALIGNED(ptr, HUGEPAGESIZE));
	}

	ret = memory_free(ptr);
	cr_assert_eq(ret, 0, "Failed to release memory: ret=%d, ptr=%p, len=%zu: %s", ret, ptr, len, strerror(errno));
}

Test(memory, manager) {
	size_t total_size;
	size_t max_block;

	int ret;
	void *p, *p1, *p2, *p3;
	struct memory_type *m;

	total_size = 1 << 10;
	max_block = total_size - sizeof(struct memory_type) - sizeof(struct memory_block);

	ret = memory_init(0);
	cr_assert(!ret);

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
