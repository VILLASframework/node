/** Unit tests for array-based list
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

void init_logging();

TestSuite(list,
	.description = "List datastructure",
	.init = init_logging
);

Test(list, list_lookup)
{
	struct list l;
	l.state = STATE_DESTROYED;

	list_init(&l);

	for (unsigned i = 0; i < ARRAY_LEN(nouns); i++) {
		struct data *d = new struct data;

		d->tag = nouns[i];
		d->data = i;

		list_push(&l, d);
	}

	struct data *found = (struct data *) list_lookup(&l, "woman");

	cr_assert_eq(found->data, 13);

	list_destroy(&l, NULL, true);
}

Test(list, list_search)
{
	struct list l;
	l.state = STATE_DESTROYED;

	list_init(&l);

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

	list_destroy(&l, NULL, false);
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
	struct list l;
	l.state = STATE_DESTROYED;

	struct content elm;
	elm.destroyed = 0;

	list_init(&l);
	list_push(&l, &elm);

	cr_assert_eq(list_length(&l), 1);

	list_destroy(&l, dtor, false);

	cr_assert_eq(elm.destroyed, 1);
}

static int compare(const void *a, const void *b) {
	return (intptr_t) b - (intptr_t) a;
}

Test(list, basics)
{
	intptr_t i;
	int ret;
	struct list l;
	l.state = STATE_DESTROYED;

	list_init(&l);

	for (i = 0; i < 100; i++) {
		cr_assert_eq(list_length(&l), i);

		list_push(&l, (void *) i);
	}

	cr_assert_eq(list_at_safe(&l, 555), NULL);
	cr_assert_eq(list_last(&l), (void *) 99);
	cr_assert_eq(list_first(&l), (void *) 0);

	for (size_t j = 0, i = 0; j < list_length(&l); j++) {
		void *k = list_at(&l, j);

		cr_assert_eq(k, (void *) i++);
	}

	list_sort(&l, compare); /* Reverse list */

	for (size_t j = 0, i = 99; j < list_length(&l); j++) {
		void *k = list_at(&l, j);

		cr_assert_eq(k, (void *) i, "Is %#zx, expected %p", i, k);
		i--;
	}

	ret = list_contains(&l, (void *) 55);
	cr_assert(ret);

	list_remove(&l, (void *) 55);

	ret = list_contains(&l, (void *) 55);
	cr_assert(!ret);

	list_destroy(&l, NULL, false);

	ret = list_length(&l);
	cr_assert_eq(ret, -1, "List not properly destroyed: l.length = %zd", l.length);
}
