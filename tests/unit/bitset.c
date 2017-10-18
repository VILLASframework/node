/** Unit tests for advio
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

#include <villas/bitset.h>
#include <villas/utils.h>

#define LEN 1027

Test(bitset, simple)
{
	int ret;
	struct bitset bs;

	int bits[] = { 23, 223, 25, 111, 252, 86, 222, 454, LEN-1 };

	ret = bitset_init(&bs, LEN);
	cr_assert_eq(ret, 0);

	for (int i = 0; i < ARRAY_LEN(bits); i++) {
		bitset_set(&bs, bits[i]);
		cr_assert_eq(ret, 0);
	}

	for (int i = 0; i < ARRAY_LEN(bits); i++) {
		ret = bitset_test(&bs, bits[i]);
		cr_assert_eq(ret, 1, "Failed at bit %d", i);
	}

	for (int i = 0; i < ARRAY_LEN(bits); i++) {
		ret = bitset_clear(&bs, bits[i]);
		cr_assert_eq(ret, 0, "Failed at bit %d", i);
	}

	for (int i = 0; i < LEN; i++) {
		ret = bitset_test(&bs, i);
		cr_assert_eq(ret, 0);
	}

	ret = bitset_destroy(&bs);
	cr_assert_eq(ret, 0);
}

Test(bitset, outofbounds)
{
	int ret;
	struct bitset bs;

	ret = bitset_init(&bs, LEN);
	cr_assert_eq(ret, 0);

	ret = bitset_set(&bs, LEN+1);
	cr_assert_eq(ret, -1);

	ret = bitset_test(&bs, LEN+1);
	cr_assert_eq(ret, -1);

	ret = bitset_destroy(&bs);
	cr_assert_eq(ret, 0);
}

Test(bitset, cmp)
{
	int ret;
	struct bitset bs1, bs2;

	ret = bitset_init(&bs1, LEN);
	cr_assert_eq(ret, 0);

	ret = bitset_init(&bs2, LEN);
	cr_assert_eq(ret, 0);

	ret = bitset_set(&bs1, 525);
	cr_assert_eq(ret, 0);

	ret = bitset_set(&bs2, 525);
	cr_assert_eq(ret, 0);

	ret = bitset_cmp(&bs1, &bs2);
	cr_assert_eq(ret, 0);

	ret = bitset_clear(&bs2, 525);
	cr_assert_eq(ret, 0);

	ret = bitset_cmp(&bs1, &bs2);
	cr_assert_neq(ret, 0);

	ret = bitset_destroy(&bs1);
	cr_assert_eq(ret, 0);

	ret = bitset_destroy(&bs2);
	cr_assert_eq(ret, 0);
}

Test(bitset, all)
{
	int ret;
	struct bitset bs;

	ret = bitset_init(&bs, LEN);
	cr_assert_eq(ret, 0);

	for (int i = 0; i < LEN; i++) {
		bitset_test(&bs, i);
		cr_assert_eq(ret, 0);
	}

	ret = bitset_destroy(&bs);
	cr_assert_eq(ret, 0);
}
