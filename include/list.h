/** A generic linked list
 *
 * Linked lists a used for several data structures in the code.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _LIST_H_
#define _LIST_H_

#include <pthread.h>

#define LIST_CHUNKSIZE		16

/** Static list initialization */
#define LIST_INIT(dtor) {			\
	.array = NULL,				\
	.length = 0,				\
	.capacity = 0,				\
	.lock = PTHREAD_MUTEX_INITIALIZER,	\
	.destructor = dtor			\
}
				
#define list_length(list)	((list)->length)
#define list_at(list, index)	((list)->length > index ? (list)->array[index] : NULL)

#define list_first(list)	list_at(list, 0)
#define list_last(list)		list_at(list, (list)->length-1)
#define list_foreach(ptr, list)	for (int _i = 0, _p; _p = 1, _i < (list)->length; _i++) \
					for (ptr = (list)->array[_i]; _p--; )

/** Callback to destroy list elements.
 *
 * @param data A pointer to the data which should be freed.
 */
typedef void (*dtor_cb_t)(void *);

/** Callback to search or sort a list. */	
typedef int (*cmp_cb_t)(const void *, const void *);

struct list {
	void **array;		/**< Array of pointers to list elements */
	size_t capacity;	/**< Size of list::array in elements */
	size_t length;		/**< Number of elements of list::array which are in use */
	dtor_cb_t destructor;	/**< A destructor which gets called for every list elements during list_destroy() */
	pthread_mutex_t lock;	/**< A mutex to allow thread-safe accesses */
};

/** Initialize a list */
void list_init(struct list *l, dtor_cb_t dtor);

/** Destroy a list and call destructors for all list elements */
void list_destroy(struct list *l);

/** Append an element to the end of the list */
void list_push(struct list *l, void *p);

/** Remove all occurences of a list item */
void list_remove(struct list *l, void *p);

/** Search the list for an element whose first element is a character array which matches name.
 *
 * List elements should be of the following form:
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

#endif /* _LIST_H_ */
