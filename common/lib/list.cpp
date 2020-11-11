/** A generic linked list
 *
 * Linked lists a used for several data structures in the code.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <array>
#include <algorithm>

#include <cstdlib>
#include <cstring>

#include <villas/list.h>
#include <villas/utils.hpp>

int vlist_init(struct vlist *l)
{
	pthread_mutex_init(&l->lock, nullptr);

	l->length = 0;
	l->capacity = 0;
	l->array = nullptr;
	l->state = State::INITIALIZED;

	return 0;
}

int vlist_destroy(struct vlist *l, dtor_cb_t destructor, bool release)
{
	pthread_mutex_lock(&l->lock);

	assert(l->state != State::DESTROYED);

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
	l->state = State::DESTROYED;

	pthread_mutex_unlock(&l->lock);
	pthread_mutex_destroy(&l->lock);

	return 0;
}

void vlist_push(struct vlist *l, void *p)
{
	pthread_mutex_lock(&l->lock);

	assert(l->state == State::INITIALIZED);

	/* Resize array if out of capacity */
	if (l->length >= l->capacity) {
		l->capacity += LIST_CHUNKSIZE;
		l->array = (void **) realloc(l->array, l->capacity * sizeof(void *));
	}

	l->array[l->length] = p;
	l->length++;

	pthread_mutex_unlock(&l->lock);
}

void vlist_clear(struct vlist *l)
{
	pthread_mutex_lock(&l->lock);

	l->length = 0;
	
	pthread_mutex_unlock(&l->lock);
}

int vlist_remove(struct vlist *l, size_t idx)
{
	pthread_mutex_lock(&l->lock);

	assert(l->state == State::INITIALIZED);

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

	assert(l->state == State::INITIALIZED);

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

	assert(l->state == State::INITIALIZED);

	for (size_t i = 0; i < vlist_length(l); i++) {
		if (vlist_at(l, i) == p)
			removed++;
		else
			l->array[i - removed] = vlist_at(l, i);
	}

	l->length -= removed;

	pthread_mutex_unlock(&l->lock);
}

int vlist_contains(struct vlist *l, void *p)
{
	return vlist_count(l, [](const void *a, const void *b) {
		return a == b ? 0 : 1;
	}, p);
}

int vlist_count(struct vlist *l, cmp_cb_t cmp, void *ctx)
{
	int c = 0;
	void *e;

	pthread_mutex_lock(&l->lock);

	assert(l->state == State::INITIALIZED);

	for (size_t i = 0; i < vlist_length(l); i++) {
		e = vlist_at(l, i);
		if (cmp(e, ctx) == 0)
			c++;
	}

	pthread_mutex_unlock(&l->lock);

	return c;
}

void * vlist_search(struct vlist *l, cmp_cb_t cmp, const void *ctx)
{
	void *e;

	pthread_mutex_lock(&l->lock);

	assert(l->state == State::INITIALIZED);

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

	assert(l->state == State::INITIALIZED);

	auto array = std::vector<void *>(l->array, l->array + l->length);

	std::sort(array.begin(), array.end(), [cmp](void *&a, void *&b) -> bool {
		return cmp(a, b) < 0;
	});

	std::copy(array.begin(), array.end(), l->array);

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

	assert(l->state == State::INITIALIZED);

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

void vlist_filter(struct vlist *l, dtor_cb_t cb)
{
	size_t i, j;
	pthread_mutex_lock(&l->lock);

	for (i  = 0, j = 0; i < vlist_length(l); i++) {
		void *e = vlist_at(l, i);

		if (!cb(e))
			l->array[j++] = e;
	}

	l->length = j;

	pthread_mutex_unlock(&l->lock);
}

int vlist_init_and_push(struct vlist *l, void *p)
{
	int ret;

	if (l->state == State::DESTROYED) {
		ret = vlist_init(l);
		if (ret)
			return ret;
	}

	vlist_push(l, p);

	return 0;
}
