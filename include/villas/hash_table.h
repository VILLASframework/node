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

#include <pthread.h>
#include <stdbool.h>

#include <villas/common.h>

struct hash_table_entry {
	void *key;
	void *data;

	struct hash_table_entry *next;
};

/** A thread-safe hash table using separate chaing with linked lists. */
struct hash_table {
	enum state state;

	struct hash_table_entry **table;

	size_t size;
	pthread_mutex_t lock;
};

/** Initialize a new hash table.
 *
 */
int hash_table_init(struct hash_table *ht, size_t size);

/** Destroy a hash table.
 *
 *
 */
int hash_table_destroy(struct hash_table *ht, dtor_cb_t dtor, bool release);

/** Insert a new key/value pair into the hash table.
 *
 */
int hash_table_insert(struct hash_table *ht, void *key, void *data);

/** Delete a key from the hash table.
 *
 *
 */
int hash_table_delete(struct hash_table *ht, void *key);

/** Perform a lookup in the hash table.
 *
 * @retval != NULL The value for the given key.
 * @retval NULL The given key is not stored in the hash table.
 */
void * hash_table_lookup(struct hash_table *ht, void *key);

/** Dump the contents of the hash table in a human readable format to stdout. */
void hash_table_dump(struct hash_table *ht);
