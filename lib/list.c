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

	l->array = NULL;
}

void list_destroy(struct list *l)
{
	pthread_mutex_lock(&l->lock);

	if (l->destructor) {
		list_foreach(void *p, l) 
			l->destructor(p);
	}
	
	free(l->array);
	
	l->array = NULL;

	l->length =
	l->capacity = 0;

	pthread_mutex_unlock(&l->lock);
	pthread_mutex_destroy(&l->lock);
}

void list_push(struct list *l, void *p)
{
	pthread_mutex_lock(&l->lock);
	
	/* Resize array if out of capacity */
	if (l->length >= l->capacity) {
		l->capacity += LIST_CHUNKSIZE;
		l->array = realloc(l->array, l->capacity * sizeof(void *));
	}
	
	l->array[l->length] = p;
	l->length++;

	pthread_mutex_unlock(&l->lock);
}

void list_remove(struct list *l, void *p)
{
	int removed = 0;

	pthread_mutex_lock(&l->lock);

	for (int i = 0; i < l->length; i++) {
		if (l->array[i] == p)
			removed++;
		else
			l->array[i - removed] = l->array[i];
	}
	
	l->length -= removed;

	pthread_mutex_unlock(&l->lock);
}

void * list_lookup(struct list *l, const char *name)
{
	int cmp_helper(const void *a, const void *b) {
		const struct {
			char *name;
		} *obj = a;
		
		return strcmp(obj->name, b);
	}

	return list_search(l, cmp_helper, (void *) name);
}

int list_contains(struct list *l, void *p)
{
	int cmp_helper(const void *a, const void *b) {
		return a == b ? 0 : 1;
	}
	
	return list_count(l, cmp_helper, p);
}

int list_count(struct list *l, cmp_cb_t cmp, void *ctx)
{
	int c = 0;

	pthread_mutex_lock(&l->lock);

	list_foreach(void *e, l) {
		if (cmp(e, ctx) == 0)
			c++;
	}

	pthread_mutex_unlock(&l->lock);

	return c;
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

	qsort(l->array, l->length, sizeof(void *), cmp_helper);

	pthread_mutex_unlock(&l->lock);
}