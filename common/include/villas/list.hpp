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
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <sys/types.h>
#include <cstring>
#include <string>
#include <uuid/uuid.h>
#include <pthread.h>

#include <villas/common.hpp>

#define LIST_CHUNKSIZE		16

/** Static list initialization */
#define LIST_INIT_STATIC(l)					\
__attribute__((constructor(105))) static void UNIQUE(__ctor)() {\
	int ret __attribute__((unused));			\
	ret = list_init(l);					\
}								\
__attribute__((destructor(105))) static void UNIQUE(__dtor)() {	\
	int ret __attribute__((unused));			\
	ret = list_destroy(l, nullptr, false);			\
}

#define list_length(list)		((list)->length)
#define list_at_safe(list, index)	((list)->length > index ? (list)->array[index] : nullptr)
#define list_at(list, index)		((list)->array[index])

#define list_first(list)		list_at(list, 0)
#define list_last(list)			list_at(list, (list)->length-1)

namespace villas {

/** Callback to search or sort a list. */
typedef int (*cmp_cb_t)(const void *, const void *);

/* The list data structure. */
struct List {
	enum State state;	/**< The state of this list. */
	void **array;		/**< Array of pointers to list elements */
	size_t capacity;	/**< Size of list::array in elements */
	size_t length;		/**< Number of elements of list::array which are in use */
	pthread_mutex_t lock;	/**< A mutex to allow thread-safe accesses */
};

/** Initialize a list.
 *
 * @param l A pointer to the list data structure.
 */
int list_init(struct List *l) __attribute__ ((warn_unused_result));

/** Destroy a list and call destructors for all list elements
 *
 * @param free free() all list members during when calling list_destroy()
 * @param dtor A function pointer to a desctructor which will be called for every list item when the list is destroyed.
 * @param l A pointer to the list data structure.
 */
int list_destroy(struct List *l, dtor_cb_t dtor = nullptr, bool free = false) __attribute__ ((warn_unused_result));

/** Append an element to the end of the list */
void list_push(struct List *l, void *p);

/** Clear list */
void list_clear(struct List *l);

/** Remove all occurences of a list item */
void list_remove_all(struct List *l, void *p);

int list_remove(struct List *l, size_t idx);

int list_insert(struct List *l, size_t idx, void *p);

/** Return the first element of the list for which cmp returns zero */
void * list_search(struct List *l, cmp_cb_t cmp, const void *ctx);

/** Returns the number of occurences for which cmp returns zero when called on all list elements. */
int list_count(struct List *l, cmp_cb_t cmp, void *ctx);

/** Return 0 if list contains pointer p */
int list_contains(struct List *l, void *p);

/** Sort the list using the quicksort algorithm of libc */
void list_sort(struct List *l, cmp_cb_t cmp);

/** Set single element in list */
int list_set(struct List *l, unsigned index, void *value);

/** Return index in list for value.
 *
 * @retval <0 No list entry  matching \p value was found.
 * @retval >=0 Entry \p value was found at returned index.
 */
ssize_t list_index(struct List *l, void *value);

/** Extend the list to the given length by filling new slots with given value. */
void list_extend(struct List *l, size_t len, void *val);

/** Remove all elements for which the callback returns a non-zero return code. */
void list_filter(struct List *l, dtor_cb_t cb);

/** Lookup an element from the list based on a name */
template<typename T>
T * list_lookup_name(struct List *l, const std::string &name)
{
	return (T *) list_search(l, [](const void *a, const void *b) -> int {
		auto *e = reinterpret_cast<const T *>(a);
		auto *s = reinterpret_cast<const std::string *>(b);

		return *s == e->name ? 0 : 1;
	}, &name);
}

/** Lookup index of list element based on name */
template<typename T>
ssize_t list_lookup_index(struct List *l, const std::string &name)
{
	auto *f = list_lookup_name<T>(l, name);

	return f ? list_index(l, f) : -1;
}

} /* namespace villas */
