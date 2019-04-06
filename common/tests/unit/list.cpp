/** Unit tests for array-based list
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdint.h>
#include <string.h>
#include <criterion/criterion.h>

#include <villas/utils.h>
#include <villas/list.h>

static const char *nouns[] = { "time", "person", "year", "way", "day", "thing", "man", "world", "life", "hand", "part", "child", "eye", "woman", "place", "work", "week", "case", "point", "government", "company", "number", "group", "problem", "fact" };

struct data {
	const char *tag;
	int data;
};

TestSuite(list, .description = "List datastructure");

Test(list, vlist_lookup)
{
	struct vlist l;
	l.state = STATE_DESTROYED;

	vlist_init(&l);

	for (unsigned i = 0; i < ARRAY_LEN(nouns); i++) {
		struct data *d = new struct data;

		d->tag = nouns[i];
		d->data = i;

		vlist_push(&l, d);
	}

	struct data *found = (struct data *) vlist_lookup(&l, "woman");

	cr_assert_eq(found->data, 13);

	vlist_destroy(&l, nullptr, true);
}

Test(list, vlist_search)
{
	struct vlist l;
	l.state = STATE_DESTROYED;

	vlist_init(&l);

	/* Fill list */
	for (unsigned i = 0; i < ARRAY_LEN(nouns); i++)
		vlist_push(&l, (void *) nouns[i]);

	cr_assert_eq(vlist_length(&l), ARRAY_LEN(nouns));

	/* Declare on stack! */
	char positive[] = "woman";
	char negative[] = "dinosaurrier";

	char *found = (char *) vlist_search(&l, (cmp_cb_t) strcmp, positive);
	cr_assert_not_null(found);
	cr_assert_eq(found, nouns[13], "found = %p, nouns[13] = %p", found, nouns[13]);
	cr_assert_str_eq(found, positive);

	char *not_found = (char *) vlist_search(&l, (cmp_cb_t) strcmp, negative);
	cr_assert_null(not_found);

	vlist_destroy(&l, nullptr, false);
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
	struct vlist l;
	l.state = STATE_DESTROYED;

	struct content elm;
	elm.destroyed = 0;

	vlist_init(&l);
	vlist_push(&l, &elm);

	cr_assert_eq(vlist_length(&l), 1);

	vlist_destroy(&l, dtor, false);

	cr_assert_eq(elm.destroyed, 1);
}

Test(list, basics)
{
	uintptr_t i;
	int ret;
	struct vlist l;
	l.state = STATE_DESTROYED;

	vlist_init(&l);

	for (i = 0; i < 100; i++) {
		cr_assert_eq(vlist_length(&l), i);

		vlist_push(&l, (void *) i);
	}

	cr_assert_eq(vlist_at_safe(&l, 555), nullptr);
	cr_assert_eq(vlist_last(&l), (void *) 99);
	cr_assert_eq(vlist_first(&l), (void *) 0);

	for (size_t j = 0, i = 0; j < vlist_length(&l); j++) {
		void *k = vlist_at(&l, j);

		cr_assert_eq(k, (void *) i++);
	}

	vlist_sort(&l, (cmp_cb_t) [](const void *a, const void *b) -> int {
		return (intptr_t) b - (intptr_t) a;
	});

	for (size_t j = 0, i = 99; j <= 99; j++, i--) {
		uintptr_t k = (uintptr_t) vlist_at(&l, j);
		cr_assert_eq(k, i, "Is %zu, expected %zu", k, i);
	}

	ret = vlist_contains(&l, (void *) 55);
	cr_assert(ret);

	void *before_ptr = vlist_at(&l, 12);

	ret = vlist_insert(&l, 12, (void *) 123);
	cr_assert_eq(ret, 0);
	cr_assert_eq(vlist_at(&l, 12), (void *) 123, "Is: %p", vlist_at(&l, 12));

	ret = vlist_remove(&l, 12);
	cr_assert_eq(ret, 0);
	cr_assert_eq(vlist_at(&l, 12), before_ptr);

	int counts, before_len;

	before_len = vlist_length(&l);
	counts = vlist_contains(&l, (void *) 55);
	cr_assert_gt(counts, 0);

	vlist_remove_all(&l, (void *) 55);
	cr_assert_eq(vlist_length(&l), (size_t) (before_len - counts));

	ret = vlist_contains(&l, (void *) 55);
	cr_assert(!ret);

	ret = vlist_destroy(&l, nullptr, false);
	cr_assert(!ret);

	ret = vlist_length(&l);
	cr_assert_eq(ret, -1, "List not properly destroyed: l.length = %zd", l.length);
}
