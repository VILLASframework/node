/** A generic linked list
 *
 * Linked lists a used for several data structures in the code.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#include <array>
#include <algorithm>

#include <cstdlib>
#include <cstring>

#include <villas/list.hpp>
#include <villas/utils.hpp>

using namespace villas;

int villas::list_init(struct List *l)
{
	pthread_mutex_init(&l->lock, nullptr);

	l->length = 0;
	l->capacity = 0;
	l->array = nullptr;
	l->state = State::INITIALIZED;

	return 0;
}

int villas::list_destroy(struct List *l, dtor_cb_t destructor, bool release)
{
	pthread_mutex_lock(&l->lock);

	assert(l->state != State::DESTROYED);

	for (size_t i = 0; i < list_length(l); i++) {
		void *e = list_at(l, i);

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

void villas::list_push(struct List *l, void *p)
{
	pthread_mutex_lock(&l->lock);

	assert(l->state == State::INITIALIZED);

	// Resize array if out of capacity
	if (l->length >= l->capacity) {
		l->capacity += LIST_CHUNKSIZE;
		l->array = (void **) realloc(l->array, l->capacity * sizeof(void *));
	}

	l->array[l->length] = p;
	l->length++;

	pthread_mutex_unlock(&l->lock);
}

void villas::list_clear(struct List *l)
{
	pthread_mutex_lock(&l->lock);

	l->length = 0;

	pthread_mutex_unlock(&l->lock);
}

int villas::list_remove(struct List *l, size_t idx)
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

int villas::list_insert(struct List *l, size_t idx, void *p)
{
	size_t i;
	void *t, *o;

	pthread_mutex_lock(&l->lock);

	assert(l->state == State::INITIALIZED);

	if (idx >= l->length)
		return -1;

	// Resize array if out of capacity
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

void villas::list_remove_all(struct List *l, void *p)
{
	int removed = 0;

	pthread_mutex_lock(&l->lock);

	assert(l->state == State::INITIALIZED);

	for (size_t i = 0; i < list_length(l); i++) {
		if (list_at(l, i) == p)
			removed++;
		else
			l->array[i - removed] = list_at(l, i);
	}

	l->length -= removed;

	pthread_mutex_unlock(&l->lock);
}

int villas::list_contains(struct List *l, void *p)
{
	return list_count(l, [](const void *a, const void *b) {
		return a == b ? 0 : 1;
	}, p);
}

int villas::list_count(struct List *l, cmp_cb_t cmp, void *ctx)
{
	int c = 0;
	void *e;

	pthread_mutex_lock(&l->lock);

	assert(l->state == State::INITIALIZED);

	for (size_t i = 0; i < list_length(l); i++) {
		e = list_at(l, i);
		if (cmp(e, ctx) == 0)
			c++;
	}

	pthread_mutex_unlock(&l->lock);

	return c;
}

void * villas::list_search(struct List *l, cmp_cb_t cmp, const void *ctx)
{
	void *e;

	pthread_mutex_lock(&l->lock);

	assert(l->state == State::INITIALIZED);

	for (size_t i = 0; i < list_length(l); i++) {
		e = list_at(l, i);
		if (cmp(e, ctx) == 0)
			goto out;
	}

	e = nullptr; // Not found

out:	pthread_mutex_unlock(&l->lock);

	return e;
}

void villas::list_sort(struct List *l, cmp_cb_t cmp)
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

int villas::list_set(struct List *l, unsigned index, void *value)
{
	if (index >= l->length)
		return -1;

	l->array[index] = value;

	return 0;
}

ssize_t villas::list_index(struct List *l, void *p)
{
	void *e;
	ssize_t f;

	pthread_mutex_lock(&l->lock);

	assert(l->state == State::INITIALIZED);

	for (size_t i = 0; i < list_length(l); i++) {
		e = list_at(l, i);
		if (e == p) {
			f = i;
			goto found;
		}
	}

	f = -1;

found:	pthread_mutex_unlock(&l->lock);

	return f;
}

void villas::list_extend(struct List *l, size_t len, void *val)
{
	while (list_length(l) < len)
		list_push(l, val);
}

void villas::list_filter(struct List *l, dtor_cb_t cb)
{
	size_t i, j;
	pthread_mutex_lock(&l->lock);

	for (i  = 0, j = 0; i < list_length(l); i++) {
		void *e = list_at(l, i);

		if (!cb(e))
			l->array[j++] = e;
	}

	l->length = j;

	pthread_mutex_unlock(&l->lock);
}
