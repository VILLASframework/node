/** A generic linked list
 *
 * Linked lists a used for several data structures in the code.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
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

#include <stdlib.h>
#include <string.h>

#include <villas/list.h>
#include <villas/utils.h>

/* Compare functions */
static int cmp_lookup(const void *a, const void *b) {
	struct name_tag {
		char *name;
	};

	const name_tag *obj = (struct name_tag *) a;

	return strcmp(obj->name, (char *) b);
}

static int cmp_contains(const void *a, const void *b) {
	return a == b ? 0 : 1;
}

#ifdef __APPLE__
static int cmp_sort(void *thunk, const void *a, const void *b) {
#else
static int cmp_sort(const void *a, const void *b, void *thunk) {
#endif
	cmp_cb_t cmp = (cmp_cb_t) thunk;

	return cmp(*(const void **) a, *(const void **) b);
}

int vlist_init(struct vlist *l)
{
	assert(l->state == STATE_DESTROYED);

	pthread_mutex_init(&l->lock, nullptr);

	l->length = 0;
	l->capacity = 0;
	l->array = nullptr;
	l->state = STATE_INITIALIZED;

	return 0;
}

int vlist_destroy(struct vlist *l, dtor_cb_t destructor, bool release)
{
	pthread_mutex_lock(&l->lock);

	assert(l->state != STATE_DESTROYED);

	for (size_t i = 0; i < vlist_length(l); i++) {
		void *e = vlist_at(l, i);

		if (destructor)
			destructor(e);
		if (release)
			free(e);
	}

	free(l->array);

	l->length = -1;
	l->capacity = 0;
	l->array = nullptr;
	l->state = STATE_DESTROYED;

	pthread_mutex_unlock(&l->lock);
	pthread_mutex_destroy(&l->lock);

	return 0;
}

void vlist_push(struct vlist *l, void *p)
{
	pthread_mutex_lock(&l->lock);

	assert(l->state == STATE_INITIALIZED);

	/* Resize array if out of capacity */
	if (l->length >= l->capacity) {
		l->capacity += LIST_CHUNKSIZE;
		l->array = (void **) realloc(l->array, l->capacity * sizeof(void *));
	}

	l->array[l->length] = p;
	l->length++;

	pthread_mutex_unlock(&l->lock);
}

int vlist_remove(struct vlist *l, size_t idx)
{
	pthread_mutex_lock(&l->lock);

	assert(l->state == STATE_INITIALIZED);

	if (idx >= l->length)
		return -1;

	for (size_t i = idx; i < l->length - 1; i++)
		l->array[i] = l->array[i+1];

	l->length--;

	pthread_mutex_unlock(&l->lock);

	return 0;
}

int vlist_insert(struct vlist *l, size_t idx, void *p)
{
	size_t i;
	void *t, *o;

	pthread_mutex_lock(&l->lock);

	assert(l->state == STATE_INITIALIZED);

	if (idx >= l->length)
		return -1;

	/* Resize array if out of capacity */
	if (l->length + 1 > l->capacity) {
		l->capacity += LIST_CHUNKSIZE;
		l->array = (void **) realloc(l->array, l->capacity * sizeof(void *));
	}

	o = p;
	for (i = idx; i < l->length; i++) {
		t = l->array[i];
		l->array[i] = o;
		o = t;
	}

	l->array[l->length] = o;
	l->length++;

	pthread_mutex_unlock(&l->lock);

	return 0;
}

void vlist_remove_all(struct vlist *l, void *p)
{
	int removed = 0;

	pthread_mutex_lock(&l->lock);

	assert(l->state == STATE_INITIALIZED);

	for (size_t i = 0; i < vlist_length(l); i++) {
		if (vlist_at(l, i) == p)
			removed++;
		else
			l->array[i - removed] = vlist_at(l, i);
	}

	l->length -= removed;

	pthread_mutex_unlock(&l->lock);
}

void * vlist_lookup(struct vlist *l, const char *name)
{
	return vlist_search(l, cmp_lookup, (void *) name);
}

ssize_t vlist_lookup_index(struct vlist *l, const char *name)
{
	void *ptr = vlist_lookup(l, name);
	if (!ptr)
		return -1;

	return vlist_index(l, ptr);
}

int vlist_contains(struct vlist *l, void *p)
{
	return vlist_count(l, cmp_contains, p);
}

int vlist_count(struct vlist *l, cmp_cb_t cmp, void *ctx)
{
	int c = 0;
	void *e;

	pthread_mutex_lock(&l->lock);

	assert(l->state == STATE_INITIALIZED);

	for (size_t i = 0; i < vlist_length(l); i++) {
		e = vlist_at(l, i);
		if (cmp(e, ctx) == 0)
			c++;
	}

	pthread_mutex_unlock(&l->lock);

	return c;
}

void * vlist_search(struct vlist *l, cmp_cb_t cmp, void *ctx)
{
	void *e;

	pthread_mutex_lock(&l->lock);

	assert(l->state == STATE_INITIALIZED);

	for (size_t i = 0; i < vlist_length(l); i++) {
		e = vlist_at(l, i);
		if (cmp(e, ctx) == 0)
			goto out;
	}

	e = nullptr; /* not found */

out:	pthread_mutex_unlock(&l->lock);

	return e;
}

void vlist_sort(struct vlist *l, cmp_cb_t cmp)
{
	pthread_mutex_lock(&l->lock);

	assert(l->state == STATE_INITIALIZED);

#ifdef __APPLE__
	qsort_r(l->array, l->length, sizeof(void *), (void *) cmp, cmp_sort);
#else
	qsort_r(l->array, l->length, sizeof(void *), cmp_sort, (void *) cmp);
#endif

	pthread_mutex_unlock(&l->lock);
}

int vlist_set(struct vlist *l, unsigned index, void *value)
{
	if (index >= l->length)
		return -1;

	l->array[index] = value;

	return 0;
}

ssize_t vlist_index(struct vlist *l, void *p)
{
	void *e;
	ssize_t f;

	pthread_mutex_lock(&l->lock);

	assert(l->state == STATE_INITIALIZED);

	for (size_t i = 0; i < vlist_length(l); i++) {
		e = vlist_at(l, i);
		if (e == p) {
			f = i;
			goto found;
		}
	}

	f = -1;

found:	pthread_mutex_unlock(&l->lock);

	return f;
}

void vlist_extend(struct vlist *l, size_t len, void *val)
{
	while (vlist_length(l) < len)
		vlist_push(l, val);
}
