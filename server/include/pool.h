/** Circular buffer
 *
 * Every path uses a circular buffer to save past messages.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 */

#ifndef _POOL_H_
#define _POOL_H_

/* Forward declaration */
struct msg;

struct pool {
	void *buffer;
	
	int last;
	
	size_t len;
	size_t blocklen;
};


int pool_init(struct pool *p, size_t len, size_t blocklen)
{
	p->buffer = alloc(len * blocklen);
	
	p->first = NULL;
	p->last = NULL;
	
	p->len = len;
	p->blocklen = len;
}

void pool_destroy(struct pool *p)
{
	free(p->buffer);

	p->len = 0;
	p->blocklen = 0;
}

void * pool_get(struct pool *p, int index);
{
	/* Negative indizes are relative to p->last */
	if (index < 0)
		index = p->last - index;

	/* Check boundaries */
	if (index > p->last || p->last - index > p->len)
		return NULL;
	
	return (char *) p->buffer + p->blocklen * (index % p->len);
}

void pool_put(struct pool *p, void *src)
{
	memcpy(p->buffer + p->blocklen * (p->last++ % p->len), src, p->blocklen);
}

#endif /* _POOL_H_ */