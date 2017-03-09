/** Unit tests for memory management
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <criterion/criterion.h>
#include <criterion/theories.h>

#include <errno.h>

#include "memory.h"
#include "utils.h"

TheoryDataPoints(memory, aligned) = {
	DataPoints(size_t, 1, 32, 55, 1 << 10, 1 << 20),
	DataPoints(size_t, 1, 8, 1 << 12),
	DataPoints(const struct memtype *, &memtype_heap, &memtype_hugepage)
};

Theory((size_t len, size_t align, const struct memtype *m), memory, aligned) {
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