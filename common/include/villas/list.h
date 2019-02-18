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
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <stdbool.h>
#include <sys/types.h>
#include <pthread.h>

#include <villas/common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LIST_CHUNKSIZE		16

/** Static list initialization */
#define LIST_INIT_STATIC(l)					\
__attribute__((constructor(105))) static void UNIQUE(__ctor)() {\
	if ((l)->state == STATE_DESTROYED)			\
		vlist_init(l);					\
}								\
__attribute__((destructor(105))) static void UNIQUE(__dtor)() {	\
	vlist_destroy(l, NULL, false);				\
}

#define vlist_length(list)	((list)->length)
#define vlist_at_safe(list, index) ((list)->length > index ? (list)->array[index] : NULL)
#define vlist_at(list, index)	((list)->array[index])

#define vlist_first(list)	vlist_at(list, 0)
#define vlist_last(list)		vlist_at(list, (list)->length-1)

/** Callback to search or sort a list. */
typedef int (*cmp_cb_t)(const void *, const void *);

/* The list data structure. */
struct vlist {
	enum state state;	/**< The state of this list. */
	void **array;		/**< Array of pointers to list elements */
	size_t capacity;	/**< Size of list::array in elements */
	size_t length;		/**< Number of elements of list::array which are in use */
	pthread_mutex_t lock;	/**< A mutex to allow thread-safe accesses */
};

/** Initialize a list.
 *
 * @param l A pointer to the list data structure.
 */
int vlist_init(struct vlist *l);

/** Destroy a list and call destructors for all list elements
 *
 * @param free free() all list members during when calling vlist_destroy()
 * @param dtor A function pointer to a desctructor which will be called for every list item when the list is destroyed.
 * @param l A pointer to the list data structure.
 */
int vlist_destroy(struct vlist *l, dtor_cb_t dtor, bool free);

/** Append an element to the end of the list */
void vlist_push(struct vlist *l, void *p);

/** Remove all occurences of a list item */
void vlist_remove(struct vlist *l, void *p);

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
void * vlist_lookup(struct vlist *l, const char *name);

ssize_t vlist_lookup_index(struct vlist *l, const char *name);

/** Return the first element of the list for which cmp returns zero */
void * vlist_search(struct vlist *l, cmp_cb_t cmp, void *ctx);

/** Returns the number of occurences for which cmp returns zero when called on all list elements. */
int vlist_count(struct vlist *l, cmp_cb_t cmp, void *ctx);

/** Return 0 if list contains pointer p */
int vlist_contains(struct vlist *l, void *p);

/** Sort the list using the quicksort algorithm of libc */
void vlist_sort(struct vlist *l, cmp_cb_t cmp);

/** Set single element in list */
int vlist_set(struct vlist *l, int index, void *value);

/** Return index in list for value.
 *
 * @retval <0 No list entry  matching \p value was found.
 * @retval >=0 Entry \p value was found at returned index.
 */
ssize_t vlist_index(struct vlist *l, void *value);

/** Extend the list to the given length by filling new slots with given value. */
void vlist_extend(struct vlist *l, size_t len, void *val);

/** Shallow copy a list. */
int vlist_copy(struct vlist *dst, const struct vlist *src);

#ifdef __cplusplus
}
#endif
