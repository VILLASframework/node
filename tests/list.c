#include <stdint.h>
#include <criterion/criterion.h>

#include "list.h"

static int compare(const void *a, const void *b) {
	return b - a;
}

Test(test, basics)
{
	intptr_t i;
	struct list l;
	
	list_init(&l);
	
	for (i = 0; i < 100; i++) {
		cr_assert_eq(list_length(&l), i);
		
		list_push(&l, (void *) i);
	}

	cr_assert_eq(list_at(&l, 555), NULL);
	cr_assert_eq(list_last(&l), (void *) 99);
	cr_assert_eq(list_first(&l), NULL);

	i = 0;
	list_foreach (void *j, &l)
		cr_assert_eq(j, (void *) i++);
	
	list_sort(&l, compare); /* Reverse list */
	
	i = 99;
	list_foreach (void *j, &l) {
		cr_assert_eq(j, (void *) i, "Is %p, expected %p", i, j);
		i--;
	}

	cr_assert(list_contains(&l, (void *) 55));
	
	list_remove(&l, (void *) 55);
	
	cr_assert(!list_contains(&l, (void *) 55));

	list_destroy(&l, NULL, false);

	cr_assert_eq(list_length(&l), -1, "List not properly destroyed: l.length = %zd", l.length);
}