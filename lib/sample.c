/** The internal datastructure for a sample of simulation data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include "pool.h"
#include "sample.h"

int sample_alloc(struct pool *p, struct sample *smps[], int cnt)
{
	int ret;

	ret = pool_get_many(p, (void **) smps, cnt);
	if (ret < 0)
		return ret;

	for (int i = 0; i < ret; i++) {
		smps[i]->capacity = (p->blocksz - sizeof(**smps)) / sizeof(smps[0]->data[0]);
		smps[i]->pool_off = (char *) p - (char *) smps[i];
		smps[i]->format = 0; /* all sample values are float by default */
	}

	return ret;
}

void sample_free(struct sample *smps[], int cnt)
{
	for (int i = 0; i < cnt; i++) {
		struct pool *p = (struct pool *) ((char *) smps[i] + smps[i]->pool_off);
		pool_put(p, smps[i]);
	}
}

int sample_get(struct sample *s)
{
	return atomic_fetch_add(&s->refcnt, 1) + 1;
}

int sample_put(struct sample *s)
{
	int prev = atomic_fetch_sub(&s->refcnt, 1);
	
	/* Did we had the last reference? */
	if (prev == 1) {
		struct pool *p = (struct pool *) ((char *) s + s->pool_off);
		pool_put(p, s);
	}
	
	return prev - 1;
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
