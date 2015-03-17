/** A generic linked list
 *
 * Linked lists a used for several data structures in the code.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#include "list.h"

void list_init(struct list *l)
{
	pthread_mutex_init(&l->lock, NULL);
	
	l->count = 0;
	l->head = NULL;
	l->tail = NULL;
}

void list_destroy(struct list *l)
{
	pthread_mutex_lock(&l->lock);
	
	struct list_elm *elm = l->head;
	while (elm) {
		struct list_elm *tmp = elm;
		free(tmp);

		elm = elm->next;
	}
	
	pthread_mutex_destroy(&l->lock);
}

void list_push(struct list *l, void *d)
{
	struct list_elm *e = alloc(sizeof(struct list_elm));
	
	pthread_mutex_lock(&l->lock);
	
	e->data = d;
	e->prev = l->tail;
	e->next = NULL;
	
	l->tail->next = e;
	l->tail = e;
	
	pthread_mutex_unlock(&l->lock);
}

struct list_elm * list_search(struct list *l, int (*cmp)(void *))
{	
	foreach(l, it) {
		if (!cmp(it->data))
			return it;
	}

	return NULL;
}
