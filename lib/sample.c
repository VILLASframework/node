/** The internal datastructure for a sample of simulation data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <ctype.h>

#include "pool.h"
#include "sample.h"
#include "timing.h"

int sample_alloc(struct pool *p, struct sample *smps[], int cnt) {
	int ret;

	ret = pool_get_many(p, (void **) smps, cnt);
	if (ret < 0)
		return ret;

	for (int i = 0; i < ret; i++) {
		smps[i]->capacity = (p->blocksz - sizeof(**smps)) / sizeof(smps[0]->data[0]);
		smps[i]->pool = p;
	}

	return ret;
}

void sample_free(struct sample *smps[], int cnt)
{
	for (int i = 0; i < cnt; i++)
		pool_put(smps[i]->pool, smps[i]);
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

int sample_print(char *buf, size_t len, struct sample *s, int flags)
{
	size_t off = snprintf(buf, len, "%llu", (unsigned long long) s->ts.origin.tv_sec);
	
	if (flags & SAMPLE_NANOSECONDS)
		off += snprintf(buf + off, len - off, ".%09llu", (unsigned long long) s->ts.origin.tv_nsec);
	
	if (flags & SAMPLE_OFFSET)
		off += snprintf(buf + off, len - off, "%+e", time_delta(&s->ts.origin, &s->ts.received));
	
	if (flags & SAMPLE_SEQUENCE)
		off += snprintf(buf + off, len - off, "(%u)", s->sequence);

	if (flags & SAMPLE_VALUES) {
		for (int i = 0; i < s->length; i++)
			off += snprintf(buf + off, len - off, "\t%.6f", s->data[i].f);
	}

	off += snprintf(buf + off, len - off, "\n");

	return off + 1; /* trailing '\0' */
}

int sample_scan(const char *line, struct sample *s, int *fl)
{
	char *end;
	const char *ptr = line;

	int flags = 0;
	double offset = 0;

	/* Format: Seconds.NanoSeconds+Offset(SequenceNumber) Value1 Value2 ... 
	 * RegEx: (\d+(?:\.\d+)?)([-+]\d+(?:\.\d+)?(?:e[+-]?\d+)?)?(?:\((\d+)\))?
	 *
	 * Please note that only the seconds and at least one value are mandatory
	 */

	/* Mandatory: seconds */
	s->ts.origin.tv_sec = (uint32_t) strtoul(ptr, &end, 10);
	if (ptr == end)
		return -2;

	/* Optional: nano seconds */
	if (*end == '.') {
		ptr = end + 1;

		s->ts.origin.tv_nsec = (uint32_t) strtoul(ptr, &end, 10);
		if (ptr != end)
			flags |= SAMPLE_NANOSECONDS;
		else
			return -3;
	}
	else
		s->ts.origin.tv_nsec = 0;

	/* Optional: offset / delay */
	if (*end == '+' || *end == '-') {
		ptr = end;

		offset = strtof(ptr, &end); /* offset is ignored for now */
		if (ptr != end)
			flags |= SAMPLE_OFFSET;
		else
			return -4;
	}
	
	/* Optional: sequence */
	if (*end == '(') {
		ptr = end + 1;
		
		s->sequence = strtoul(ptr, &end, 10);
		if (ptr != end)
			flags |= SAMPLE_SEQUENCE;
		else
			return -5;
		
		if (*end == ')')
			end++;
	}

	for (ptr  = end, s->length = 0;
	                 s->length < s->capacity;
	     ptr  = end, s->length++) {

		s->data[s->length].f = strtod(ptr, &end); /** @todo We only support floating point values at the moment */
		if (end == ptr) /* There are no valid FP values anymore */
			break;
	}
	
	if (s->length > 0)
		flags |= SAMPLE_VALUES;
	
	if (fl)
		*fl = flags;
	if (flags & SAMPLE_OFFSET) {
		struct timespec off = time_from_double(offset);
		s->ts.received = time_diff(&s->ts.origin, &off);
	}

	return s->length;
}

int sample_fprint(FILE *f, struct sample *s, int flags)
{
	char line[4096];

	int len = sample_print(line, sizeof(line), s, flags);
	
	fputs(line, f);
	
	return len;
}

int sample_fscan(FILE *f, struct sample *s, int *fl)
{
	char *ptr, line[4096];
	
skip:	if (fgets(line, sizeof(line), f) == NULL)
		return -1; /* An error occured */

	/* Skip whitespaces, empty and comment lines */
	for (ptr = line; isspace(*ptr); ptr++);
	if (*ptr == '\0' || *ptr == '#')
		goto skip;
	
	return sample_scan(line, s, fl);
}