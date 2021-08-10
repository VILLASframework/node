/** Managed memory allocator.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <strings.h>

#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <villas/node/memory.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/log.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::memory;

static
struct Allocation * managed_alloc(size_t len, size_t alignment, struct Type *m)
{
	/* Simple first-fit allocation */
	struct Block *first = (struct Block *) m->_vd;
	struct Block *block;

	for (block = first; block != nullptr; block = block->next) {
		if (block->used)
			continue;

		char* cptr = (char *) block + sizeof(struct Block);
		size_t avail = block->length;
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
			if (gap > sizeof(struct Block)) {
				/* The alignment gap is big enough to fit another block.
				 * The original block descriptor is already at the correct
				 * position, so we just change its len and create a new block
				 * descriptor for the actual block we're handling. */
				block->length = gap - sizeof(struct Block);
				struct Block *newblock = (struct Block *) (cptr - sizeof(struct Block));
				newblock->prev = block;
				newblock->next = block->next;
				block->next = newblock;
				newblock->used = false;
				newblock->length = len;
				block = newblock;
			}
			else {
				/* The gap is too small to fit another block descriptor, so we
				 * must account for the gap length in the block length. */
				block->length = len + gap;
			}

			if (avail > len + sizeof(struct Block)) {
				/* Imperfect fit, so create another block for the remaining part */
				struct Block *newblock = (struct Block *) (cptr + len);
				newblock->prev = block;
				newblock->next = block->next;
				block->next = newblock;

				if (newblock->next)
					newblock->next->prev = newblock;

				newblock->used = false;
				newblock->length = avail - len - sizeof(struct Block);
			}
			else {
				/* If this block was larger than the requested length, but only
				 * by less than sizeof(struct Block), we may have wasted
				 * memory by previous assignments to block->length. */
				block->length = avail;
			}

			block->used = true;

			auto *ma = new struct Allocation;
			if (!ma)
				throw MemoryAllocationError();

			ma->address = cptr;
			ma->type = m;
			ma->alignment = alignment;
			ma->length = len;
			ma->managed.block = block;

			return ma;
		}
	}

	/* No suitable block found */
	return nullptr;
}

static
int managed_free(struct Allocation *ma, struct Type *m)
{
	struct Block *block = ma->managed.block;

	/* Try to merge it with neighbouring free blocks */
	if (block->prev && !block->prev->used &&
	    block->next && !block->next->used) {
		/* Special case first: both previous and next block are unused */
		block->prev->length += block->length + block->next->length + 2 * sizeof(struct Block);
		block->prev->next = block->next->next;
		if (block->next->next)
			block->next->next->prev = block->prev;
	}
	else if (block->prev && !block->prev->used) {
		block->prev->length += block->length + sizeof(struct Block);
		block->prev->next = block->next;
		if (block->next)
			block->next->prev = block->prev;
	}
	else if (block->next && !block->next->used) {
		block->length += block->next->length + sizeof(struct Block);
		block->next = block->next->next;
		if (block->next)
			block->next->prev = block;
	}
	else {
		/* no neighbouring free block, so just mark it as free */
		block->used = false;
	}

	return 0;
}

struct Type * villas::node::memory::managed(void *ptr, size_t len)
{
	struct Type *mt = (struct Type *) ptr;
	struct Block *mb;
	char *cptr = (char *) ptr;

	if (len < sizeof(struct Type) + sizeof(struct Block)) {
		auto logger = logging.get("memory:managed");
		logger->info("Passed region is too small");
		return nullptr;
	}

	/* Initialize type */
	mt->name  = "managed";
	mt->flags = 0;
	mt->alloc = managed_alloc;
	mt->free  = managed_free;
	mt->alignment = 1;

	cptr += ALIGN(sizeof(struct Type), sizeof(void *));

	/* Initialize first free memory block */
	mb = (struct Block *) cptr;
	mb->prev = nullptr;
	mb->next = nullptr;
	mb->used = false;

	cptr += ALIGN(sizeof(struct Block), sizeof(void *));

	mb->length = len - (cptr - (char *) ptr);

	mt->_vd = (void *) mb;

	return mt;
}
