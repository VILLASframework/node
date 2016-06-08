/** The internal datastructure for a sample of simulation data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <ctype.h>

#include "sample.h"
#include "timing.h"

int sample_print(char *buf, size_t len, struct sample *s, int flags)
{
	size_t off = snprintf(buf, len, "%llu", (unsigned long long) s->ts.origin.tv_sec);
	
	if (flags & SAMPLE_NANOSECONDS)
		off += snprintf(buf + off, len - off, ".%09llu", (unsigned long long) s->ts.origin.tv_nsec);
	
	if (flags & SAMPLE_OFFSET)
		off += snprintf(buf + off, len - off, "%+g", time_delta(&s->ts.received, &s->ts.origin));
	
	if (flags & SAMPLE_SEQUENCE)
		off += snprintf(buf + off, len - off, "(%u)", s->sequence);

	if (flags & SAMPLE_VALUES) {
		for (int i = 0; i < s->length; i++)
			off += snprintf(buf + off, len - off, "\t%.6f", s->values[i].f);
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

	for (s->length = 0, ptr  = end; ;
	     s->length++,   ptr = end) {

		/** @todo We only support floating point values at the moment */
		s->values[s->length].f = strtod(ptr, &end);

		if (end == ptr) /* there are no valid FP values anymore */
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