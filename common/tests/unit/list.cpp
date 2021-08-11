/** Unit tests for array-based list
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#include <cstdint>
#include <cstring>
#include <criterion/criterion.h>

#include <villas/exceptions.hpp>
#include <villas/utils.hpp>
#include <villas/list.hpp>

using namespace villas;

static const char *nouns[] = { "time", "person", "year", "way", "day", "thing", "man", "world", "life", "hand", "part", "child", "eye", "woman", "place", "work", "week", "case", "point", "government", "company", "number", "group", "problem", "fact" };

struct data {
	const char *name;
	int data;
};

// cppcheck-suppress unknownMacro
TestSuite(list, .description = "List datastructure");

Test(list, list_search)
{
	int ret;
	struct List l;

	ret = list_init(&l);
	cr_assert_eq(ret, 0);

	/* Fill list */
	for (unsigned i = 0; i < ARRAY_LEN(nouns); i++)
		list_push(&l, (void *) nouns[i]);

	cr_assert_eq(list_length(&l), ARRAY_LEN(nouns));

	/* Declare on stack! */
	char positive[] = "woman";
	char negative[] = "dinosaurrier";

	char *found = (char *) list_search(&l, (cmp_cb_t) strcmp, positive);
	cr_assert_not_null(found);
	cr_assert_eq(found, nouns[13], "found = %p, nouns[13] = %p", found, nouns[13]);
	cr_assert_str_eq(found, positive);

	char *not_found = (char *) list_search(&l, (cmp_cb_t) strcmp, negative);
	cr_assert_null(not_found);

	ret = list_destroy(&l, nullptr, false);
	cr_assert_eq(ret, 0);
}

struct content {
	int destroyed;
};

static int dtor(void *ptr)
{
	struct content *elm = (struct content *) ptr;

	elm->destroyed = 1;

	return 0;
}

Test(list, destructor)
{
	int ret;
	struct List l;

	struct content elm;
	elm.destroyed = 0;

	ret = list_init(&l);
	cr_assert_eq(ret, 0);

	list_push(&l, &elm);

	cr_assert_eq(list_length(&l), 1);

	ret = list_destroy(&l, dtor, false);
	cr_assert_eq(ret, 0);

	cr_assert_eq(elm.destroyed, 1);
}

Test(list, basics)
{
	uintptr_t i;
	int ret;
	struct List l;

	ret = list_init(&l);
	cr_assert_eq(ret, 0);

	for (i = 0; i < 100; i++) {
		cr_assert_eq(list_length(&l), i);

		list_push(&l, (void *) i);
	}

	cr_assert_eq(list_at_safe(&l, 555), nullptr);
	cr_assert_eq(list_last(&l), (void *) 99);
	cr_assert_eq(list_first(&l), (void *) 0);

	for (size_t j = 0, i = 0; j < list_length(&l); j++) {
		void *k = list_at(&l, j);

		cr_assert_eq(k, (void *) i++);
	}

	list_sort(&l, (cmp_cb_t) [](const void *a, const void *b) -> int {
		return (intptr_t) b - (intptr_t) a;
	});

	for (size_t j = 0, i = 99; j <= 99; j++, i--) {
		uintptr_t k = (uintptr_t) list_at(&l, j);
		cr_assert_eq(k, i, "Is %zu, expected %zu", k, i);
	}

	ret = list_contains(&l, (void *) 55);
	cr_assert(ret);

	void *before_ptr = list_at(&l, 12);

	ret = list_insert(&l, 12, (void *) 123);
	cr_assert_eq(ret, 0);
	cr_assert_eq(list_at(&l, 12), (void *) 123, "Is: %p", list_at(&l, 12));

	ret = list_remove(&l, 12);
	cr_assert_eq(ret, 0);
	cr_assert_eq(list_at(&l, 12), before_ptr);

	int counts, before_len;

	before_len = list_length(&l);
	counts = list_contains(&l, (void *) 55);
	cr_assert_gt(counts, 0);

	list_remove_all(&l, (void *) 55);
	cr_assert_eq(list_length(&l), (size_t) (before_len - counts));

	ret = list_contains(&l, (void *) 55);
	cr_assert(!ret);

	ret = list_destroy(&l, nullptr, false);
	cr_assert(!ret);

	ret = list_length(&l);
	cr_assert_eq(ret, -1, "List not properly destroyed: l.length = %zd", l.length);
}
