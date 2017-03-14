/** Unit tests for utilities
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

Test(utils, sha1sum)
{
	int ret;
	FILE *f = tmpfile();
	
	unsigned char     hash[SHA_DIGEST_LENGTH];
	unsigned char expected[SHA_DIGEST_LENGTH] = { 0x69, 0xdf, 0x29, 0xdf, 0x1f, 0xf2, 0xd2, 0x5d, 0xb8, 0x68, 0x6c, 0x02, 0x8d, 0xdf, 0x40, 0xaf, 0xb3, 0xc1, 0xc9, 0x4d };
	
	/* Write the first 512 fibonaccia numbers to the file */
	for (int i = 0, a = 0, b = 1, c; i < 512; i++, a = b, b = c) {
		c = a + b;
		
		fwrite((void *) &c, sizeof(c), 1, f);
	}

	ret = sha1sum(f, hash);

	cr_assert_eq(ret, 0);
	cr_assert_arr_eq(hash, expected, SHA_DIGEST_LENGTH);

	fclose(f);
}