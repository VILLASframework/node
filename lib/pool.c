/** Circular buffer
 *
 * Every path uses a circular buffer to save past messages.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 */

#include <stddef.h>

#include "utils.h"
#include "pool.h"

void pool_create(struct pool *p, size_t length, size_t stride)
{
	p->buffer = alloc(length * stride);
	
	p->last = 0;
	
	p->length = length;
	p->stride = stride;
}

void pool_destroy(struct pool *p)
{
	free(p->buffer);

	p->length =
	p->stride = 0;
}

void pool_push(struct pool *p, int blocks)
{
	p->previous = p->last;
	p->last += blocks;
}

void * pool_get(struct pool *p, int index)
{
	return (char *) p->buffer + p->stride * (index % p->length);
}

void * pool_getrel(struct pool *p, int offset)
{
	return pool_get(p, p->last + offset);
}
