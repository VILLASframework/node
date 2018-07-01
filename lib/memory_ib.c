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

#include <villas/nodes/infiniband.h>
#include <villas/memory_ib.h>
#include <rdma/rdma_cma.h>

struct ibv_mr * memory_ib_mr(void *ptr)
{
	struct ibv_mr *mr = (struct ibv_mr *) ptr;

	return (mr - 1);
}

void * memory_ib_alloc(struct memtype *m, size_t len, size_t alignment)
{
	struct memory_ib *mi = (struct memory_ib *) m->_vd;

	struct ibv_mr **mr = memory_alloc_aligned(mi->parent, len + sizeof(struct ibv_mr *), alignment);
	char *ptr = (char *) (mr + 1);

	*mr = ibv_reg_mr(mi->pd, ptr, len, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
	if(!*mr) {
		free(ptr);
		return NULL;
	}

	return ptr;
}

int memory_ib_free(struct memtype *m, void *ptr, size_t len)
{
	struct memory_ib *mi = (struct memory_ib *) m->_vd;
	struct ibv_mr *mr = memory_ib_mr(ptr);

	ibv_dereg_mr(mr);

	ptr -= sizeof(struct ibv_mr *);
	len += sizeof(struct ibv_mr *);

	memory_free(mi->parent, ptr, len);

	return 0;
}

struct memtype * ib_memtype(struct node *n, struct memtype *parent)
{
	struct infiniband *i = (struct infiniband *) n->_vd;
	struct memtype *mt = malloc(sizeof(struct memtype));

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
