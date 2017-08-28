/** A generic list implementation.
 *
 * This is a generic implementation of a list which can store void pointers to
 * arbitrary data. The data itself is not stored or managed by the list.
 *
 * Internally, an array of pointers is used to store the pointers.
 * If needed, this array will grow by realloc().
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 20177, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#include <stdbool.h>
#include <pthread.h>

#include "common.h"

#define LIST_CHUNKSIZE		16

/** Static list initialization */
#define LIST_INIT() {				\
	.array = NULL,				\
	.length = 0,				\
	.capacity = 0,				\
	.lock = PTHREAD_MUTEX_INITIALIZER,	\
	.state = STATE_INITIALIZED		\
}

#define LIST_INIT_STATIC(l)					\
__attribute__((constructor(105))) static void UNIQUE(__ctor)() {\
	if ((l)->state == STATE_DESTROYED)			\
		list_init(l);					\
}								\
__attribute__((destructor(105))) static void UNIQUE(__dtor)() {	\
	list_destroy(l, NULL, false);				\
}

#define list_length(list)	((list)->length)
#define list_at_safe(list, index) ((list)->length > index ? (list)->array[index] : NULL)
#define list_at(list, index)	((list)->array[index])

#define list_first(list)	list_at(list, 0)
#define list_last(list)		list_at(list, (list)->length-1)

/** Callback to destroy list elements.
 *
 * @param data A pointer to the data which should be freed.
 */
typedef int (*dtor_cb_t)(void *);

/** Callback to search or sort a list. */
typedef int (*cmp_cb_t)(const void *, const void *);

/* The list data structure. */
struct list {
	void **array;		/**< Array of pointers to list elements */
	size_t capacity;	/**< Size of list::array in elements */
	size_t length;		/**< Number of elements of list::array which are in use */
	pthread_mutex_t lock;	/**< A mutex to allow thread-safe accesses */
	enum state state;	/**< The state of this list. */
};

/** Initialize a list.
 *
 * @param l A pointer to the list data structure.
 */
int list_init(struct list *l);

/** Destroy a list and call destructors for all list elements
 *
 * @param free free() all list members during when calling list_destroy()
 * @param dtor A function pointer to a desctructor which will be called for every list item when the list is destroyed.
 * @param l A pointer to the list data structure.
 */
int list_destroy(struct list *l, dtor_cb_t dtor, bool free);

/** Append an element to the end of the list */
void list_push(struct list *l, void *p);

/** Remove all occurences of a list item */
void list_remove(struct list *l, void *p);

/** Return the first list element which is identified by a string in its first member variable.
 *
 * List elements are pointers to structures of the following form:
 *
 * struct obj {
 *    char *name;
 *    // more members
 * }
 *
 * @see Only possible because of §1424 of http://c0x.coding-guidelines.com/6.7.2.1.html
 */
void * list_lookup(struct list *l, const char *name);

/** Return the first element of the list for which cmp returns zero */
void * list_search(struct list *l, cmp_cb_t cmp, void *ctx);

/** Returns the number of occurences for which cmp returns zero when called on all list elements. */
int list_count(struct list *l, cmp_cb_t cmp, void *ctx);

/** Return 0 if list contains pointer p */
int list_contains(struct list *l, void *p);

/** Sort the list using the quicksort algorithm of libc */
void list_sort(struct list *l, cmp_cb_t cmp);

/** Set single element in list */
int list_set(struct list *l, int index, void *value);
