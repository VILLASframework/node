/** The internal datastructure for a sample of simulation data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <string.h>

#include "pool.h"
#include "sample.h"
#include "utils.h"

int sample_alloc(struct pool *p, struct sample *smps[], int cnt)
{
	int ret;

	ret = pool_get_many(p, (void **) smps, cnt);
	if (ret < 0)
		return ret;

	for (int i = 0; i < ret; i++) {
		smps[i]->capacity = (p->blocksz - SAMPLE_LEN(0)) / SAMPLE_DATA_LEN(1);
		smps[i]->pool = p;
		smps[i]->format = 0; /* all sample values are float by default */
		smps[i]->refcnt = ATOMIC_VAR_INIT(1);
	}

	return ret;
}

void sample_free(struct sample *smps[], int cnt)
{
	for (int i = 0; i < cnt; i++)
		pool_put(smps[i]->pool, smps[i]);
}

int sample_put_many(struct sample *smps[], int cnt)
{
	int released = 0;
	
	for (int i = 0; i < cnt; i++) {
		if (sample_put(smps[i]) == 0)
			released++;
	}
	
	return released;
}

int sample_get_many(struct sample *smps[], int cnt)
{
	for (int i = 0; i < cnt; i++)
		sample_get(smps[i]);
	
	return cnt;
}

int sample_get(struct sample *s)
{
	return atomic_fetch_add(&s->refcnt, 1) + 1;
}

int sample_put(struct sample *s)
{
	int prev = atomic_fetch_sub(&s->refcnt, 1);
	
	/* Did we had the last refernce? */
	if (prev == 1)
		pool_put(s->pool, s);
	
	return prev - 1;
}

int sample_copy(struct sample *dst, struct sample *src)
{
	dst->length = MIN(src->length, dst->capacity);

	dst->sequence = src->sequence;
	dst->format = src->format;
	dst->source = src->source;
	
	dst->ts.origin   = src->ts.origin;
	dst->ts.received = src->ts.received;
	dst->ts.sent     = src->ts.sent;

	memcpy(&dst->data, &src->data, SAMPLE_DATA_LEN(dst->length));
	
	return 0;
}

int sample_copy_many(struct sample *dsts[], struct sample *srcs[], int cnt)
{
	for (int i = 0; i < cnt; i++)
		sample_copy(dsts[i], srcs[i]);
	
	return cnt;
}

int sample_set_data_format(struct sample *s, int idx, enum sample_data_format fmt)
{
	if (idx >= sizeof(s->format) * 8)
		return 0; /* we currently can only control the format of the first 64 values. */
	
	switch (fmt) {
		case SAMPLE_DATA_FORMAT_FLOAT: s->format &= ~(1   << idx); break;
		case SAMPLE_DATA_FORMAT_INT:   s->format |=  (fmt << idx); break;
	}
	
	return 0;
}

int sample_get_data_format(struct sample *s, int idx)
{
	if (idx >= sizeof(s->format) * 8)
		return -1; /* we currently can only control the format of the first 64 values. */
	
	return (s->format >> idx) & 0x1;
}