/** A datastructure storing bitsets of arbitrary dimensions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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
#include <limits.h>

#include "bitset.h"
#include "utils.h"

#define bitset_mask(b)		(1 << ((b) % CHAR_BIT))
#define bitset_slot(b)		((b) / CHAR_BIT)
#define bitset_nslots(nb)	((nb + CHAR_BIT - 1) / CHAR_BIT)

int bitset_init(struct bitset *b, size_t dim)
{
	int s = bitset_nslots(dim);

	b->set = alloc(s * CHAR_BIT);
	b->dimension = dim;

	return 0;
}

int bitset_destroy(struct bitset *b)
{
	free(b->set);

	return 0;
}

void bitset_set_value(struct bitset *b, uintmax_t val)
{
	bitset_clear_all(b);

	for (size_t i = 0; i < b->dim; i++) {
		if (val & (1 << i))
			bitset_set(b, i);
	}
}

uintmax_t bitset_get_value(struct bitset *b)
{
	uintmax_t v = 0;

	for (size_t i = 0; i < b->dim; i++)
		v += bitset_test(b, i) << i;

	return v;
}

size_t bitset_count(struct bitset *b)
{
	size_t cnt = 0;

	for (size_t i = 0; i < b->dim; i++)
		cnt += bitset_test(b, i);

	return cnt;
}

int bitset_set(struct bitset *b, size_t bit)
{
	int s = bitset_slot(bit);
	char m = bitset_mask(bit);

	if (bit >= b->dimension)
		return -1;

	b->set[s] |= m;

	return 0;
}

int bitset_clear(struct bitset *b, size_t bit)
{
	int s = bitset_slot(bit);
	char m = bitset_mask(bit);

	if (bit >= b->dimension)
		return -1;

	b->set[s] &= ~m;

	return 0;
}

int bitset_test(struct bitset *b, size_t bit)
{
	int s = bitset_slot(bit);
	char m = bitset_mask(bit);

	if (bit >= b->dimension)
		return -1;

	return b->set[s] & m ? 1 : 0;
}

void bitset_set_all(struct bitset *b)
{
	int s = b->dimension / CHAR_BIT;

	if (b->dimension % CHAR_BIT) {
		char m = (1 << (b->dimension % CHAR_BIT)) - 1;

		b->set[s] |= m;
	}

	memset(b->set, ~0, s);
}

void bitset_clear_all(struct bitset *b)
{
	int s = b->dimension / CHAR_BIT;

	/* Clear last byte */
	if (b->dimension % CHAR_BIT) {
		char m = (1 << (b->dimension % CHAR_BIT)) - 1;

		b->set[s] &= ~m;
	}

	memset(b->set, 0x00, s);
}

int bitset_cmp(struct bitset *a, struct bitset *b)
{
	int s = a->dimension / CHAR_BIT;

	if (a->dimension != b->dimension)
		return -1;

	/* Compare last byte with mask */
	if (a->dimension % CHAR_BIT) {
		char m = (1 << (a->dimension % CHAR_BIT)) - 1;

		if ((a->set[s] & m) != (b->set[s] & m))
			return -1;
	}

	return memcmp(a->set, b->set, s);
}

char * bitset_dump(struct bitset *b)
{
	char *str = NULL;

	for (int i = 0; i < b->dimension; i++)
		strcatf(&str, "%d", bitset_test(b, i));

	return str;
}
