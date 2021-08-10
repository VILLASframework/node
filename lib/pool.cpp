/** Memory pool for fixed size objects.
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

#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/pool.hpp>
#include <villas/node/memory.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/log.hpp>

using namespace villas;

int villas::node::pool_init(struct Pool *p, size_t cnt, size_t blocksz, struct memory::Type *m)
{
	int ret;

	/* Make sure that we use a block size that is aligned to the size of a cache line */
	p->alignment = kernel::getCachelineSize();
	p->blocksz = p->alignment * CEIL(blocksz, p->alignment);
	p->len = cnt * p->blocksz;

	void *buffer = memory::alloc_aligned(p->len, p->alignment, m);
	if (!buffer)
		throw MemoryAllocationError();

	auto logger = logging.get("pool");
	logger->debug("Allocated {:#x} bytes for memory pool", p->len);

	p->buffer_off = (char*) buffer - (char*) p;

	ret = queue_init(&p->queue, LOG2_CEIL(cnt), m);
	if (ret)
		return ret;

	for (unsigned i = 0; i < cnt; i++)
		queue_push(&p->queue, (char *) buffer + i * p->blocksz);

	p->state = State::INITIALIZED;

	return 0;
}

int villas::node::pool_destroy(struct Pool *p)
{
	int ret;

	if (p->state == State::DESTROYED)
		return 0;

	ret = queue_destroy(&p->queue);
	if (ret)
		return ret;

	void *buffer = (char *) p + p->buffer_off;
	ret = memory::free(buffer);
	if (ret == 0)
		p->state = State::DESTROYED;

	return ret;
}
