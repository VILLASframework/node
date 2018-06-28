/** A datastructure storing bitsets of arbitrary dimensions.
 *
 * @file
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

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bitset {
	char *set;
	size_t dimension;
};

/** Allocate memory for a new betset */
int bitset_init(struct bitset *b, size_t dim);

/** Release memory of bit set */
int bitset_destroy(struct bitset *b);

void bitset_set_value(struct bitset *b, uintmax_t val);
uintmax_t bitset_get_value(struct bitset *b);

/** Return the number of bits int the set which are set (1) */
size_t bitset_count(struct bitset *b);

/** Set a single bit in the set */
int bitset_set(struct bitset *b, size_t bit);

/** Clear a single bit in the set */
int bitset_clear(struct bitset *b, size_t bit);

/** Set all bits in the set */
void bitset_set_all(struct bitset *b);

/** Clear all bits in the set */
void bitset_clear_all(struct bitset *b);

/** Test if a single bit in the set is set */
int bitset_test(struct bitset *b, size_t bit);

/** Compare two bit sets bit-by-bit */
int bitset_cmp(struct bitset *a, struct bitset *b);

/** Return an human readable representation of the bit set */
char *bitset_dump(struct bitset *b);

#ifdef __cplusplus
}
#endif
