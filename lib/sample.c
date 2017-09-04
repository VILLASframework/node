/** The internal datastructure for a sample of simulation data.
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

#include <string.h>
#include <math.h>

#include "pool.h"
#include "sample.h"
#include "utils.h"
#include "timing.h"

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
		smps[i]->refcnt = ATOMIC_VAR_INIT(1);
	}

	return ret;
}

void sample_free(struct sample *smps[], int cnt)
{
	for (int i = 0; i < cnt; i++)
		pool_put(sample_pool(smps[i]), smps[i]);
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

	/* Did we had the last reference? */
	if (prev == 1)
		pool_put(sample_pool(s), s);

	return prev - 1;
}

int sample_copy(struct sample *dst, struct sample *src)
{
	dst->length = MIN(src->length, dst->capacity);

	dst->sequence = src->sequence;
	dst->format = src->format;
	dst->source = src->source;
	dst->has = src->has;

	dst->ts = src->ts;

	memcpy(&dst->data, &src->data, SAMPLE_DATA_LEN(dst->length));

	return 0;
}

int sample_copy_many(struct sample *dsts[], struct sample *srcs[], int cnt)
{
	for (int i = 0; i < cnt; i++)
		sample_copy(dsts[i], srcs[i]);

	return cnt;
}

int sample_cmp(struct sample *a, struct sample *b, double epsilon, int flags)
{
	if ((a->has & b->has & flags) != flags) {
		printf("missing components: a=%#x, b=%#x, wanted=%#x\n", a->has, b->has, flags);
		return -1;
	}

	/* Compare sequence no */
	if (flags & SAMPLE_SEQUENCE) {
		if (a->sequence != b->sequence) {
			printf("sequence no: %d != %d\n", a->sequence, b->sequence);
			return 2;
		}
	}

	/* Compare timestamp */
	if (flags & SAMPLE_ORIGIN) {
		if (time_delta(&a->ts.origin, &b->ts.origin) > epsilon) {
			printf("ts.origin: %f != %f\n", time_to_double(&a->ts.origin), time_to_double(&b->ts.origin));
			return 3;
		}
	}

	/* Compare ID */
	if (flags & SAMPLE_SOURCE) {
		if (a->id != b->id) {
			printf("id: %d != %d\n", a->id, b->id);
			return 7;
		}
	}

	/* Compare data */
	if (flags & SAMPLE_VALUES) {
		if (a->length != b->length) {
			printf("length: %d != %d\n", a->length, b->length);
			return 4;
		}

		if (a->format != b->format) {
			printf("format: %#lx != %#lx\n", a->format, b->format);
			return 6;
		}

		for (int i = 0; i < a->length; i++) {
			switch (sample_get_data_format(a, i)) {
				case SAMPLE_DATA_FORMAT_FLOAT:
					if (fabs(a->data[i].f - b->data[i].f) > epsilon) {
						printf("data[%d].f: %f != %f\n", i, a->data[i].f, b->data[i].f);
						return 5;
					}
					break;

				case SAMPLE_DATA_FORMAT_INT:
					if (a->data[i].i != b->data[i].i) {
						printf("data[%d].i: %ld != %ld\n", i, a->data[i].i, b->data[i].i);
						return 5;
					}
					break;
			}
		}
	}

	return 0;
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
