/** Unit tests for hash table
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <criterion/criterion.h>

#include <villas/utils.h>
#include <villas/hash_table.h>

static char *keys[]   = { "able",   "achieve", "acoustics", "action", "activity", "aftermath", "afternoon", "afterthought", "apparel",  "appliance", "beginner", "believe",   "bomb",   "border",  "boundary",   "breakfast", "cabbage",  "cable",  "calculator", "calendar",  "caption", "carpenter", "cemetery", "channel", "circle", "creator", "creature", "education", "faucet", "feather", "friction", "fruit",     "fuel",    "galley", "guide",    "guitar", "health", "heart",   "idea",   "kitten",  "laborer",    "language" };
static char *values[] = { "lawyer", "linen",   "locket",    "lumber", "magic",    "minister",  "mitten",    "money",        "mountain", "music",     "partner",  "passenger", "pickle", "picture", "plantation", "plastic",   "pleasure", "pocket", "police",     "pollution", "railway", "recess",    "reward",   "route",   "scene",  "scent",   "squirrel", "stranger",  "suit",   "sweater", "temper",   "territory", "texture", "thread", "treatment", "veil",  "vein",   "volcano", "wealth", "weather", "wilderness", "wren" };

Test(hash_table, hash_table_lookup)
{
	int ret;
	struct hash_table ht = { .state = STATE_DESTROYED };

	ret = hash_table_init(&ht, 20);
	cr_assert(!ret);

	/* Insert */
	for (int i = 0; i < ARRAY_LEN(keys); i++) {
		ret = hash_table_insert(&ht, keys[i], values[i]);
		cr_assert(!ret);
	}

	/* Lookup */
	for (int i = 0; i < ARRAY_LEN(keys); i++) {
		char *value = hash_table_lookup(&ht, keys[i]);
		cr_assert_eq(values[i], value);
	}

	/* Inserting the same key twice should fail */
	ret = hash_table_insert(&ht, keys[0], values[0]);
	cr_assert(ret);

	hash_table_dump(&ht);

	/* Removing an entry */
	ret = hash_table_delete(&ht, keys[0]);
	cr_assert(!ret);

	/* Removing the same entry twice should fail */
	ret = hash_table_delete(&ht, keys[0]);
	cr_assert(ret);

	/* After removing, we should be able to insert it again */
	ret = hash_table_insert(&ht, keys[0], values[0]);
	cr_assert(!ret);

	ret = hash_table_destroy(&ht, NULL, false);
	cr_assert(!ret);
}
