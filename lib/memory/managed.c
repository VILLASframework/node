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
#include <unistd.h>
#include <errno.h>
#include <strings.h>

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <villas/log.h>
#include <villas/memory.h>
#include <villas/utils.h>

void* memory_managed_alloc(struct memory_type *m, size_t len, size_t alignment)
{
	/* Simple first-fit allocation */
	struct memblock *first = (struct memblock *) m->_vd;
	struct memblock *block;

	for (block = first; block != NULL; block = block->next) {
		if (block->flags & MEMBLOCK_USED)
			continue;

		char* cptr = (char *) block + sizeof(struct memblock);
		size_t avail = block->len;
		uintptr_t uptr = (uintptr_t) cptr;

		/* Check alignment first; leave a gap at start of block to assure
		 * alignment if necessary */
		uintptr_t rem = uptr % alignment;
		uintptr_t gap = 0;
		if (rem != 0) {
			gap = alignment - rem;
			if (gap > avail)
				continue; /* Next aligned address isn't in this block anymore */

			cptr += gap;
			avail -= gap;
		}

		if (avail >= len) {
			if (gap > sizeof(struct memblock)) {
				/* The alignment gap is big enough to fit another block.
				 * The original block descriptor is already at the correct
				 * position, so we just change its len and create a new block
				 * descriptor for the actual block we're handling. */
				block->len = gap - sizeof(struct memblock);
				struct memblock *newblock = (struct memblock *) (cptr - sizeof(struct memblock));
				newblock->prev = block;
				newblock->next = block->next;
				block->next = newblock;
				newblock->flags = 0;
				newblock->len = len;
				block = newblock;
			}
			else {
				/* The gap is too small to fit another block descriptor, so we
				 * must account for the gap length in the block length. */
				block->len = len + gap;
			}

			if (avail > len + sizeof(struct memblock)) {
				/* Imperfect fit, so create another block for the remaining part */
				struct memblock *newblock = (struct memblock *) (cptr + len);
				newblock->prev = block;
				newblock->next = block->next;
				block->next = newblock;
				if (newblock->next)
					newblock->next->prev = newblock;
				newblock->flags = 0;
				newblock->len = avail - len - sizeof(struct memblock);
			}
			else {
				/* If this block was larger than the requested length, but only
				 * by less than sizeof(struct memblock), we may have wasted
				 * memory by previous assignments to block->len. */
				block->len = avail;
			}

			block->flags |= MEMBLOCK_USED;

			return (void *) cptr;
		}
	}

	/* No suitable block found */
	return NULL;
}

int memory_managed_free(struct memory_type *m, void *ptr, size_t len)
{
	struct memblock *first = (struct memblock *) m->_vd;
	struct memblock *block;
	char *cptr = ptr;

	for (block = first; block != NULL; block = block->next) {
		if (!(block->flags & MEMBLOCK_USED))
			continue;

		/* Since we may waste some memory at the start of a block to ensure
		 * alignment, ptr may not actually be the start of the block */
		if ((char *) block + sizeof(struct memblock) <= cptr &&
		    cptr < (char *) block + sizeof(struct memblock) + block->len) {
			/* Try to merge it with neighbouring free blocks */
			if (block->prev && !(block->prev->flags & MEMBLOCK_USED) &&
			    block->next && !(block->next->flags & MEMBLOCK_USED)) {
				/* Special case first: both previous and next block are unused */
				block->prev->len += block->len + block->next->len + 2 * sizeof(struct memblock);
				block->prev->next = block->next->next;
				if (block->next->next)
					block->next->next->prev = block->prev;
			}
			else if (block->prev && !(block->prev->flags & MEMBLOCK_USED)) {
				block->prev->len += block->len + sizeof(struct memblock);
				block->prev->next = block->next;
				if (block->next)
					block->next->prev = block->prev;
			}
			else if (block->next && !(block->next->flags & MEMBLOCK_USED)) {
				block->len += block->next->len + sizeof(struct memblock);
				block->next = block->next->next;
				if (block->next)
					block->next->prev = block;
			}
			else {
				/* no neighbouring free block, so just mark it as free */
				block->flags &= ~MEMBLOCK_USED;
			}

			return 0;
		}
	}

	return -1;
}

struct memory_type * memory_managed(void *ptr, size_t len)
{
	struct memory_type *mt = ptr;
	struct memblock *mb;
	char *cptr = ptr;

	if (len < sizeof(struct memory_type) + sizeof(struct memblock)) {
		info("memory_managed: passed region too small");
		return NULL;
	}

	/* Initialize memory_type */
	mt->name  = "managed";
	mt->flags = 0;
	mt->alloc = memory_managed_alloc;
	mt->free  = memory_managed_free;
	mt->alignment = 1;

	cptr += ALIGN(sizeof(struct memory_type), sizeof(void *));

	/* Initialize first free memblock */
	mb = (struct memblock *) cptr;
	mb->prev = NULL;
	mb->next = NULL;
	mb->flags = 0;

	cptr += ALIGN(sizeof(struct memblock), sizeof(void *));

	mb->len = len - (cptr - (char *) ptr);

	mt->_vd = (void *) mb;

	return mt;
}
