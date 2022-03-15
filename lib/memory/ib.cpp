/** Infiniband memory allocator.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <infiniband/verbs.h>

#include <villas/nodes/infiniband.hpp>
#include <villas/node/memory.hpp>
#include <villas/utils.hpp>
#include <villas/memory/ib.h>
#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>

using namespace villas;
using namespace villas::utils;
using namespace villas::node;
using namespace villas::node::memory;

struct ibv_mr * villas::node::memory::ib_get_mr(void *ptr)
{
	auto *ma = get_allocation(ptr);

	return ma->ib.mr;
}

static
struct Allocation * ib_alloc(size_t len, size_t alignment, struct Type *m)
{
	auto *mi = (struct IB *) m->_vd;

	auto *ma = new struct Allocation;
	if (!ma)
		throw MemoryAllocationError();

	ma->type = m;
	ma->length = len;
	ma->alignment = alignment;

	ma->parent = mi->parent->alloc(len + sizeof(struct ibv_mr *), alignment, mi->parent);
	ma->address = ma->parent->address;

	if (!mi->pd) {
		auto logger = logging.get("memory:ib");
		logger->error("Protection domain is not registered!");
	}

	ma->ib.mr = ibv_reg_mr(mi->pd, ma->address, ma->length, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
	if (!ma->ib.mr) {
		mi->parent->free(ma->parent, mi->parent);
		delete ma;
		return nullptr;
	}

	return ma;
}

static
int ib_free(struct Allocation *ma, struct Type *m)
{
	int ret;
	auto *mi = (struct IB *) m->_vd;

	ibv_dereg_mr(ma->ib.mr);

	ret = mi->parent->free(ma->parent, mi->parent);
	if (ret)
		return ret;

	return 0;
}

struct Type * villas::node::memory::ib(NodeCompat *n, struct Type *parent)
{
	auto *i = n->getData<struct infiniband>();
	auto *mt = (struct Type *) malloc(sizeof(struct Type));

	mt->name = "ib";
	mt->flags = 0;
	mt->alloc = ib_alloc;
	mt->free = ib_free;
	mt->alignment = 1;

	mt->_vd = malloc(sizeof(struct IB));

	auto *mi = (struct memory::IB *) mt->_vd;

	mi->pd = i->ctx.pd;
	mi->parent = parent;

	return mt;
}
