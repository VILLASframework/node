/** A generic hash table
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
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

#include <string.h>

#include <villas/utils.h>
#include <villas/hash_table.h>

static int hash_table_hash(struct hash_table *ht, void *key)
{
	uintptr_t ptr = (uintptr_t) key;

	return ptr % ht->size;
}

int hash_table_init(struct hash_table *ht, size_t size)
{
	int ret;
	size_t len = sizeof(struct hash_table_entry *) * size;

	assert(ht->state == STATE_DESTROYED);

	ret = pthread_mutex_init(&ht->lock, NULL);
	if (ret)
		return ret;

	ht->table = alloc(len);

	memset(ht->table, 0, len);

	ht->size = size;
	ht->state = STATE_INITIALIZED;

	return 0;
}

int hash_table_destroy(struct hash_table *ht, dtor_cb_t dtor, bool release)
{
	int ret;
	struct hash_table_entry *cur, *next;

	assert(ht->state == STATE_INITIALIZED);

	pthread_mutex_lock(&ht->lock);

	for (int i = 0; i < ht->size; i++) {
		for (cur = ht->table[i]; cur; cur = next) {
			if (dtor)
				dtor(cur->data);

			if (release)
				free(cur->data);

			next = cur->next;

			free(cur);
		}
	}

	pthread_mutex_unlock(&ht->lock);

	ret = pthread_mutex_destroy(&ht->lock);
	if (ret)
		return ret;

	free(ht->table);

	ht->state = STATE_DESTROYED;

	return 0;
}

int hash_table_insert(struct hash_table *ht, void *key, void *data)
{
	int ret, ikey = hash_table_hash(ht, key);
	struct hash_table_entry *hte, *cur;

	assert(ht->state == STATE_INITIALIZED);

	pthread_mutex_lock(&ht->lock);

	/* Check that the key is not already in the table */
	for (cur = ht->table[ikey];
	     cur && cur->key != key;
	     cur = cur->next);

	if (cur)
		ret = -1;
	else {
		hte = alloc(sizeof(struct hash_table_entry));
		if (hte) {
			hte->key = key;
			hte->data = data;
			hte->next = NULL;

			if ((cur = ht->table[ikey]))
				hte->next = ht->table[ikey];

			ht->table[ikey] = hte;

			ret = 0;
		}
		else
			ret = -1;
	}

	pthread_mutex_unlock(&ht->lock);

	return ret;
}

#include <stdio.h>

int hash_table_delete(struct hash_table *ht, void *key)
{
	int ret, ikey = hash_table_hash(ht, key);
	struct hash_table_entry *cur, *prev;

	assert(ht->state == STATE_INITIALIZED);

	pthread_mutex_lock(&ht->lock);

	for (prev = NULL,
	     cur = ht->table[ikey];
	     cur && cur->key != key;
	     prev = cur,
	     cur = cur->next);

	if (cur) {
		if (prev)
			prev->next = cur->next;
		else
			ht->table[ikey] = cur->next;

		free(cur);

		ret = 0;
	}
	else
		ret = -1; /* not found */

	pthread_mutex_unlock(&ht->lock);

	return ret;
}

void * hash_table_lookup(struct hash_table *ht, void *key)
{
	int ikey = hash_table_hash(ht, key);
	struct hash_table_entry *hte;

	assert(ht->state == STATE_INITIALIZED);

	pthread_mutex_lock(&ht->lock);

	for (hte = ht->table[ikey];
	     hte && hte->key != key;
	     hte = hte->next);

	void *data = hte ? hte->data : NULL;

	pthread_mutex_unlock(&ht->lock);

	return data;
}

void hash_table_dump(struct hash_table *ht)
{
	struct hash_table_entry *hte;

	assert(ht->state == STATE_INITIALIZED);

	pthread_mutex_lock(&ht->lock);

	for (int i = 0; i < ht->size; i++) {
		char *strlst = NULL;

		for (hte = ht->table[i]; hte; hte = hte->next)
			strcatf(&strlst, "%p->%p ", hte->key, hte->data);

		info("%i: %s", i, strlst);
		free(strlst);
	}

	pthread_mutex_unlock(&ht->lock);
}
