/** Unit tests for memory management
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <criterion/criterion.h>
#include <criterion/theories.h>

#include <errno.h>

#include "memory.h"
#include "utils.h"

TheoryDataPoints(memory, aligned) = {
	DataPoints(size_t, 1, 32, 55, 1 << 10, 1 << 20),
	DataPoints(size_t, 1, 8, 1 << 12),
	DataPoints(struct memtype *, &memtype_heap, &memtype_hugepage)
};

Theory((size_t len, size_t align, struct memtype *m), memory, aligned) {
	int ret;
	void *ptr;
	
	ptr = memory_alloc_aligned(m, len, align);
	cr_assert_neq(ptr, NULL, "Failed to allocate memory");
	
	cr_assert(IS_ALIGNED(ptr, align));

	if (m == &memtype_hugepage) {
		cr_assert(IS_ALIGNED(ptr, HUGEPAGESIZE));
	}

	ret = memory_free(m, ptr, len);
	cr_assert_eq(ret, 0, "Failed to release memory: ret=%d, ptr=%p, len=%zu: %s", ret, ptr, len, strerror(errno));
}

Test(memory, manager) {
	size_t total_size;
	size_t max_block;
	
	int ret;
	void *p, *p1, *p2, *p3;
	struct memtype *m;

	total_size = 1 << 10;
	max_block = total_size - sizeof(struct memtype) - sizeof(struct memblock);

	p = memory_alloc(&memtype_heap, total_size);
	cr_assert_not_null(p);
	
	m = memtype_managed_init(p, total_size);
	cr_assert_not_null(m);

	p1 = memory_alloc(m, 16);
	cr_assert_not_null(p1);

	p2 = memory_alloc(m, 32);
	cr_assert_not_null(p2);

	ret = memory_free(m, p1, 16);
	cr_assert(ret == 0);

	p1 = memory_alloc_aligned(m, 128, 128);
	cr_assert_not_null(p1);
	cr_assert(IS_ALIGNED(p1, 128));

	p3 = memory_alloc_aligned(m, 128, 256);
	cr_assert(p3);
	cr_assert(IS_ALIGNED(p3, 256));

	ret = memory_free(m, p2, 32);
	cr_assert(ret == 0);

	ret = memory_free(m, p1, 128);
	cr_assert(ret == 0);

	ret = memory_free(m, p3, 128);
	cr_assert(ret == 0);

	p1 = memory_alloc(m, max_block);
	cr_assert_not_null(p1);

	ret = memory_free(m, p1, max_block);
	cr_assert(ret == 0);

	ret = memory_free(&memtype_heap, p, total_size);
	cr_assert(ret == 0);
}
