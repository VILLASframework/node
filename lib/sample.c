/** The internal datastructure for a sample of simulation data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
#include <inttypes.h>

#include <villas/pool.h>
#include <villas/sample.h>
#include <villas/utils.h>
#include <villas/timing.h>
#include <villas/signal.h>
#include <villas/list.h>

int sample_init(struct sample *s)
{
	struct pool *p = sample_pool(s);

	s->length = 0;
	s->capacity = (p->blocksz - sizeof(struct sample)) / sizeof(s->data[0]);
	s->refcnt = ATOMIC_VAR_INIT(1);

	return 0;
}

struct sample * sample_alloc(struct pool *p)
{
	struct sample *s;

	s = pool_get(p);
	if (!s)
		return NULL;

	s->pool_off = (char *) p - (char *) s;

	sample_init(s);

	return s;
}

struct sample * sample_alloc_mem(int capacity)
{
	size_t sz = SAMPLE_LENGTH(capacity);

	char *b = alloc(sz);
	if (!b)
		return NULL;

	struct sample *s = (struct sample *) b;

	s->pool_off = SAMPLE_NON_POOL;

	s->length = 0;
	s->capacity = capacity;
	s->refcnt = ATOMIC_VAR_INIT(1);

	return s;
}

void sample_free(struct sample *s)
{
	struct pool *p = sample_pool(s);

	if (p)
		pool_put(p, s);
	else
		free(s);
}

int sample_alloc_many(struct pool *p, struct sample *smps[], int cnt)
{
	int ret;

	ret = pool_get_many(p, (void **) smps, cnt);
	if (ret < 0)
		return ret;

	for (int i = 0; i < ret; i++) {
		smps[i]->pool_off = (char *) p - (char *) smps[i];

		sample_init(smps[i]);
	}

	return ret;
}

void sample_free_many(struct sample *smps[], int cnt)
{
	for (int i = 0; i < cnt; i++)
		sample_free(smps[i]);
}

int sample_decref_many(struct sample *smps[], int cnt)
{
	int released = 0;

	for (int i = 0; i < cnt; i++) {
		if (sample_decref(smps[i]) == 0)
			released++;
	}

	return released;
}

int sample_incref_many(struct sample *smps[], int cnt)
{
	for (int i = 0; i < cnt; i++)
		sample_incref(smps[i]);

	return cnt;
}

int sample_incref(struct sample *s)
{
	return atomic_fetch_add(&s->refcnt, 1) + 1;
}

int sample_decref(struct sample *s)
{
	int prev = atomic_fetch_sub(&s->refcnt, 1);

	/* Did we had the last reference? */
	if (prev == 1)
		sample_free(s);

	return prev - 1;
}

int sample_copy(struct sample *dst, struct sample *src)
{
	dst->length = MIN(src->length, dst->capacity);

	dst->sequence = src->sequence;
	dst->flags = src->flags;
	dst->ts = src->ts;
	dst->signals = src->signals;

	memcpy(&dst->data, &src->data, SAMPLE_DATA_LENGTH(dst->length));

	return 0;
}

struct sample * sample_clone(struct sample *orig)
{
	struct sample *clone;
	struct pool *pool;

	pool = sample_pool(orig);
	if (!pool)
		return NULL;

	clone = sample_alloc(pool);
	if (!clone)
		return NULL;

	sample_copy(clone, orig);

	return clone;
}

int sample_clone_many(struct sample *clones[], struct sample *origs[], int cnt)
{
	int alloced, copied;
	struct pool *pool;

	if (cnt <= 0)
		return 0;

	pool = sample_pool(origs[0]);
	if (!pool)
		return 0;

	alloced = sample_alloc_many(pool, clones, cnt);

	copied = sample_copy_many(clones, origs, alloced);

	return copied;
}

int sample_copy_many(struct sample *dsts[], struct sample *srcs[], int cnt)
{
	for (int i = 0; i < cnt; i++)
		sample_copy(dsts[i], srcs[i]);

	return cnt;
}

int sample_cmp(struct sample *a, struct sample *b, double epsilon, int flags)
{
	if ((a->flags & b->flags & flags) != flags) {
		printf("flags: a=%#x, b=%#x, wanted=%#x\n", a->flags, b->flags, flags);
		return -1;
	}

	/* Compare sequence no */
	if (flags & SAMPLE_HAS_SEQUENCE) {
		if (a->sequence != b->sequence) {
			printf("sequence no: %" PRIu64 " != %" PRIu64 "\n", a->sequence, b->sequence);
			return 2;
		}
	}

	/* Compare timestamp */
	if (flags & SAMPLE_HAS_TS_ORIGIN) {
		if (time_delta(&a->ts.origin, &b->ts.origin) > epsilon) {
			printf("ts.origin: %f != %f\n", time_to_double(&a->ts.origin), time_to_double(&b->ts.origin));
			return 3;
		}
	}

	/* Compare data */
	if (flags & SAMPLE_HAS_DATA) {
		if (a->length != b->length) {
			printf("length: %d != %d\n", a->length, b->length);
			return 4;
		}

		for (int i = 0; i < a->length; i++) {
			/* Compare format */
			if (sample_format(a, i) != sample_format(b, i))
				return 6;

			switch (sample_format(a, i)) {
				case SIGNAL_TYPE_FLOAT:
					if (fabs(a->data[i].f - b->data[i].f) > epsilon) {
						printf("data[%d].f: %f != %f\n", i, a->data[i].f, b->data[i].f);
						return 5;
					}
					break;

				case SIGNAL_TYPE_INTEGER:
					if (a->data[i].i != b->data[i].i) {
						printf("data[%d].i: %" PRId64 " != %" PRId64 "\n", i, a->data[i].i, b->data[i].i);
						return 5;
					}
					break;

				case SIGNAL_TYPE_BOOLEAN:
					if (a->data[i].b != b->data[i].b) {
						printf("data[%d].b: %s != %s\n", i, a->data[i].b ? "true" : "false", b->data[i].b ? "true" : "false");
						return 5;
					}
					break;

				case SIGNAL_TYPE_COMPLEX:
					if (cabs(a->data[i].z - b->data[i].z) > epsilon) {
						printf("data[%d].z: %f+%fi != %f+%fi\n", i, creal(a->data[i].z), cimag(a->data[i].z), creal(b->data[i].z), cimag(b->data[i].z));
						return 5;
					}
					break;

				default: { }
			}
		}
	}

	return 0;
}

enum signal_type sample_format(const struct sample *s, unsigned idx)
{
	struct signal *sig;

	sig = (struct signal *) vlist_at_safe(s->signals, idx);

	return sig ? sig->type : SIGNAL_TYPE_AUTO;
}

void sample_dump(struct sample *s)
{
	debug(5, "Sample: sequence=%" PRIu64 ", length=%d, capacity=%d, flags=%#x, signals=%p, #signals=%zu, "
		"refcnt=%d, pool_off=%zd",
		s->sequence, s->length, s->capacity, s->flags, s->signals,
		s->signals ? vlist_length(s->signals) : 0, atomic_load(&s->refcnt), s->pool_off);

	if (s->flags & SAMPLE_HAS_TS_ORIGIN)
		debug(5, "  ts.origin=%ld.%ld", s->ts.origin.tv_sec, s->ts.origin.tv_nsec);

	if (s->flags & SAMPLE_HAS_TS_RECEIVED)
		debug(5, "  ts.received=%ld.%ld", s->ts.received.tv_sec, s->ts.received.tv_nsec);

	if (s->signals)
		signal_list_dump(s->signals, s->data, s->length);
}

void sample_data_insert(struct sample *smp, const union signal_data *src, size_t offset, size_t len)
{
	memmove(&smp->data[offset + len], &smp->data[offset], sizeof(smp->data[0]) * (smp->length - offset));
	memcpy(&smp->data[offset], src, sizeof(smp->data[0]) * len);

	smp->length += len;
}

void sample_data_remove(struct sample *smp, size_t offset, size_t len)
{
	size_t sz = sizeof(smp->data[0]) * len;

	memmove(&smp->data[offset], &smp->data[offset + len], sz);

	smp->length -= len;
}
