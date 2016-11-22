/** Unit tests for utilities
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <criterion/criterion.h>

#include "utils.h"

Test(utils, is_aligned)
{
	/* Positive */
	cr_assert(IS_ALIGNED(1, 1));
	cr_assert(IS_ALIGNED(128, 64));
	
	/* Negative */
	cr_assert(!IS_ALIGNED(55, 16));
	cr_assert(!IS_ALIGNED(55, 55));
	cr_assert(!IS_ALIGNED(1128, 256));
}

Test(utils, ceil)
{
	cr_assert_eq(CEIL(10, 3), 4);
	cr_assert_eq(CEIL(10, 5), 2);
	cr_assert_eq(CEIL(4, 3), 2);
}

Test(utils, is_pow2)
{
	/* Positive */
	cr_assert(IS_POW2(1));
	cr_assert(IS_POW2(2));
	cr_assert(IS_POW2(64));
	
	/* Negative */
	cr_assert(!IS_POW2(0));
	cr_assert(!IS_POW2(3));
	cr_assert(!IS_POW2(11111));
	cr_assert(!IS_POW2(-1));
}

struct version_param {
	const char *v1, *v2;
	int result;
};

Test(utils, version)
{
	struct version v1, v2, v3, v4;

	version_parse("1.2", &v1);
	version_parse("1.3", &v2);
	version_parse("55",  &v3);
	version_parse("66",  &v4);
	
	cr_assert_lt(version_cmp(&v1, &v2), 0);
	cr_assert_eq(version_cmp(&v1, &v1), 0);
	cr_assert_gt(version_cmp(&v2, &v1), 0);
	cr_assert_lt(version_cmp(&v3, &v4), 0);
}