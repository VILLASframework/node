/** A generic linked list
 *
 * Linked lists a used for several data structures in the code.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#include "utils.h"
#include "list.h"

void list_init(struct list *l, dtor_cb_t dtor)
{
	pthread_mutex_init(&l->lock, NULL);
	
	l->destructor = dtor;
	l->length = 0;
	l->head = NULL;
	l->tail = NULL;
}

void list_destroy(struct list *l)
{
	pthread_mutex_lock(&l->lock);
	
	struct list_elm *elm = l->head;
	while (elm) {
		struct list_elm *tmp = elm;
		elm = elm->next;

		if (l->destructor)
			l->destructor(tmp->ptr);

		free(tmp);
	}
	
	pthread_mutex_unlock(&l->lock);
	pthread_mutex_destroy(&l->lock);
}

void list_push(struct list *l, void *p)
{
	struct list_elm *e = alloc(sizeof(struct list_elm));
	
	pthread_mutex_lock(&l->lock);
	
	e->ptr = p;
	e->prev = l->tail;
	e->next = NULL;
	
	if (l->tail)
		l->tail->next = e;
	if (l->head)
		l->head->prev = e;
	else
		l->head = e;

	l->tail = e;
	
	l->length++;

	pthread_mutex_unlock(&l->lock);
}

struct list_elm * list_search(struct list *l, int (*cmp)(void *))
{	
	FOREACH(l, it) {
		if (!cmp(it->ptr))
			return it;
	}

	return NULL;
}
