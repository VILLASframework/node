/** Infiniband memory allocator.
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

#include <rdma/rdma_cma.h>

#include <villas/nodes/infiniband.h>
#include <villas/memory.h>
#include <villas/utils.h>
#include <villas/memory/ib.h>

struct ibv_mr * memory_ib_get_mr(void *ptr)
{
	struct memory_allocation *ma;
	struct ibv_mr *mr;

	ma = memory_get_allocation(ptr);
	mr = ma->ib.mr;

	return mr;
}

static struct memory_allocation * memory_ib_alloc(struct memory_type *m, size_t len, size_t alignment)
{
	struct memory_ib *mi = (struct memory_ib *) m->_vd;

	struct memory_allocation *ma = alloc(sizeof(struct memory_allocation));
	if (!ma)
		return NULL;

	ma->type = m;
	ma->length = len;
	ma->alignment = alignment;

	ma->parent = mi->parent->alloc(mi->parent, len + sizeof(struct ibv_mr *), alignment);
	ma->address = ma->parent->address;

	if(!mi->pd)
		error("Protection domain is not registered!");

	ma->ib.mr = ibv_reg_mr(mi->pd, ma->address, ma->length, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
	if(!ma->ib.mr) {
		mi->parent->free(mi->parent, ma->parent);
		free(ma);
		return NULL;
	}

	return ma;
}

static int memory_ib_free(struct memory_type *m, struct memory_allocation *ma)
{
	int ret;
	struct memory_ib *mi = (struct memory_ib *) m->_vd;

	ibv_dereg_mr(ma->ib.mr);

	ret = mi->parent->free(mi->parent, ma->parent);
	if (ret)
		return ret;

	return 0;
}

struct memory_type * memory_ib(struct node *n, struct memory_type *parent)
{
	struct infiniband *i = (struct infiniband *) n->_vd;
	struct memory_type *mt = malloc(sizeof(struct memory_type));

	mt->name = "ib";
	mt->flags = 0;
	mt->alloc = memory_ib_alloc;
	mt->free = memory_ib_free;
	mt->alignment = 1;

	mt->_vd = malloc(sizeof(struct memory_ib));

	struct memory_ib *mi = (struct memory_ib *) mt->_vd;

	mi->pd = i->ctx.pd;
	mi->parent = parent;

	return mt;
}
