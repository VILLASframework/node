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

#include "hooks.h"

/* Forward declarations */
struct list_elm;
struct node;
struct path;
struct interface;

/** Static list initialization */
#define LIST_INIT { \
	.head = NULL, \
	.tail = NULL, \
	.length = 0, \
	.lock = PTHREAD_MUTEX_INITIALIZER \
}

#define FOREACH(list, elm) \
	for ( struct list_elm *elm = (list)->head; \
		elm; elm = elm->next )
			
#define FOREACH_R(list, elm) \
	for ( struct list_elm *elm = (list)->tail; \
		elm; elm = elm->prev )
				
#define list_first(list)	((list)->head)
#define list_last(list)		((list)->head)
#define list_length(list)	((list)->length)

/** Callback to destroy list elements.
 *
 * @param data A pointer to the data which should be freed.
 */
typedef void (*dtor_cb_t)(void *data);

struct list {
	struct list_elm *head, *tail;
	int length;

	dtor_cb_t destructor;
	pthread_mutex_t lock;
};

struct list_elm {
	union {
		void *ptr;
		struct node *node;
		struct path *path;
		struct interface *interface;
		struct socket *socket;
		struct opal *opal;
		struct gtfpga *gtfpga;
		hook_cb_t hook;
	} /* anonymous */;

	struct list_elm *prev, *next;
};

void list_init(struct list *l, dtor_cb_t dtor);

void list_destroy(struct list *l);

void list_push(struct list *l, void *p);

struct list_elm * list_search(struct list *l, int (*cmp)(void *));

#endif /* _LIST_H_ */
