/** A generic linked list
 *
 * Linked lists a used for several data structures in the code.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "list.h"

void list_init(struct list *l, dtor_cb_t dtor)
{
	pthread_mutex_init(&l->lock, NULL);

	l->destructor = dtor;
	l->length = 0;
	l->capacity = 0;

	l->start = NULL;
	l->end = NULL;
}

void list_destroy(struct list *l)
{
	pthread_mutex_lock(&l->lock);

	if (l->destructor) {
		list_foreach(void *p, l) 
			l->destructor(p);
	}
	
	free(l->start);
	
	l->start =
	l->end = NULL;

	l->length =
	l->capacity = 0;

	pthread_mutex_unlock(&l->lock);
	pthread_mutex_destroy(&l->lock);
}

void list_push(struct list *l, void *p)
{
	pthread_mutex_lock(&l->lock);
	
	/* Resize array if out of capacity */
	if (l->end == l->start + l->capacity) {
		l->capacity += LIST_CHUNKSIZE;
		l->start = realloc(l->start, l->capacity * sizeof(void *));
	}
	
	l->start[l->length] = p;
	l->length++;
	l->end = &l->start[l->length];

	pthread_mutex_unlock(&l->lock);
}

void * list_lookup(struct list *l, const char *name)
{
	int cmp(const void *a, const void *b) {
		return strcmp(*(char **) a, b);
	}

	return list_search(l, cmp, (void *) name);
}

void * list_search(struct list *l, cmp_cb_t cmp, void *ctx)
{
	pthread_mutex_lock(&l->lock);
	
	void *e;
	list_foreach(e, l) {
		if (!cmp(e, ctx))
			goto out;
	}
	
	e = NULL; /* not found */

out:	pthread_mutex_unlock(&l->lock);

	return e;
}

void list_sort(struct list *l, cmp_cb_t cmp)
{
	int cmp_helper(const void *a, const void *b) {
		return cmp(*(void **) a, *(void **) b);
	}

	pthread_mutex_lock(&l->lock);
	qsort(l->start, l->length, sizeof(void *), cmp_helper);
	pthread_mutex_unlock(&l->lock);
}