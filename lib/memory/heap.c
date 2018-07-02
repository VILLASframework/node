/** Memory allocators.
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

#include <stdlib.h>

#include <villas/memory_type.h>

static void * memory_heap_alloc(struct memory_type *m, size_t len, size_t alignment)
{
	void *ptr;
	int ret;

	if (alignment < sizeof(void *))
		alignment = sizeof(void *);

	ret = posix_memalign(&ptr, alignment, len);

	return ret ? NULL : ptr;
}

int memory_heap_free(struct memory_type *m, void *ptr, size_t len)
{
	free(ptr);

	return 0;
}

/* List of available memory types */
struct memory_type memory_type_heap = {
	.name = "heap",
	.flags = MEMORY_HEAP,
	.alloc = memory_heap_alloc,
	.free = memory_heap_free,
	.alignment = 1
};
