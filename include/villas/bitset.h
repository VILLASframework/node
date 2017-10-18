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

struct bitset {
	char *set;
	size_t dimension;
};

int bitset_init(struct bitset *b, size_t dim);
int bitset_destroy(struct bitset *b);


int bitset_set(struct bitset *b, size_t bit);
int bitset_clear(struct bitset *b, size_t bit);

void bitset_set_all(struct bitset *b);
void bitset_clear_all(struct bitset *b);

int bitset_test(struct bitset *b, size_t bit);

int bitset_cmp(struct bitset *a, struct bitset *b);

char * bitset_dump(struct bitset *b);
