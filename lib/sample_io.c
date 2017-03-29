/** The internal datastructure for a sample of simulation data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <ctype.h>

#include "sample.h"
#include "sample_io.h"
#include "timing.h"

int sample_io_fprint(FILE *f, struct sample *s, enum sample_io_format fmt, int flags)
{
	switch (fmt) {
		case SAMPLE_IO_FORMAT_VILLAS: return sample_io_villas_fprint(f, s, flags);
		case SAMPLE_IO_FORMAT_JSON:   return sample_io_json_fprint(f, s, flags);
		default:
			return -1;
	}
}

int sample_io_fscan(FILE *f, struct sample *s, enum sample_io_format fmt, int *flags)
{
	switch (fmt) {
		case SAMPLE_IO_FORMAT_VILLAS: return sample_io_villas_fscan(f, s, flags);
		case SAMPLE_IO_FORMAT_JSON: return sample_io_json_fscan(f, s, flags);
		default:
			return -1;
	}
}

int sample_io_villas_print(char *buf, size_t len, struct sample *s, int flags)
{
	size_t off = snprintf(buf, len, "%llu", (unsigned long long) s->ts.origin.tv_sec);
	
	if (flags & SAMPLE_IO_NANOSECONDS)
		off += snprintf(buf + off, len - off, ".%09llu", (unsigned long long) s->ts.origin.tv_nsec);
	
	if (flags & SAMPLE_IO_OFFSET)
		off += snprintf(buf + off, len - off, "%+e", time_delta(&s->ts.origin, &s->ts.received));
	
	if (flags & SAMPLE_IO_SEQUENCE)
		off += snprintf(buf + off, len - off, "(%u)", s->sequence);

	if (flags & SAMPLE_IO_VALUES) {
		for (int i = 0; i < s->length; i++) {
			switch ((s->format >> i) & 0x1) {
				case SAMPLE_DATA_FORMAT_FLOAT:
					off += snprintf(buf + off, len - off, "\t%.6f", s->data[i].f);
					break;
				case SAMPLE_DATA_FORMAT_INT:
					off += snprintf(buf + off, len - off, "\t%d", s->data[i].i);
					break;
			}
		}
	}

	off += snprintf(buf + off, len - off, "\n");

	return 0; /* trailing '\0' */
}

int sample_io_villas_scan(const char *line, struct sample *s, int *fl)
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
			flags |= SAMPLE_IO_NANOSECONDS;
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
			flags |= SAMPLE_IO_OFFSET;
		else
			return -4;
	}
	
	/* Optional: sequence */
	if (*end == '(') {
		ptr = end + 1;
		
		s->sequence = strtoul(ptr, &end, 10);
		if (ptr != end)
			flags |= SAMPLE_IO_SEQUENCE;
		else
			return -5;
		
		if (*end == ')')
			end++;
	}

	for (ptr  = end, s->length = 0;
	                 s->length < s->capacity;
	     ptr  = end, s->length++) {

		switch (s->format & (1 << s->length)) {
			case SAMPLE_DATA_FORMAT_FLOAT:
				s->data[s->length].f = strtod(ptr, &end);
				break;
			case SAMPLE_DATA_FORMAT_INT:
				s->data[s->length].i = strtol(ptr, &end, 10);
				break;
		}

		if (end == ptr) /* There are no valid FP values anymore */
			break;
	}
	
	if (s->length > 0)
		flags |= SAMPLE_IO_VALUES;
	
	if (fl)
		*fl = flags;
	if (flags & SAMPLE_IO_OFFSET) {
		struct timespec off = time_from_double(offset);
		s->ts.received = time_add(&s->ts.origin, &off);
	}
	else
		s->ts.received = s->ts.origin;

	return 0;
}

int sample_io_villas_fprint(FILE *f, struct sample *s, int flags)
{
	char line[4096];
	int ret;
	
	ret = sample_io_villas_print(line, sizeof(line), s, flags);
	if (ret)
		return ret;

	fputs(line, f);
	
	return 0;
}

int sample_io_villas_fscan(FILE *f, struct sample *s, int *fl)
{
	char *ptr, line[4096];
	
skip:	if (fgets(line, sizeof(line), f) == NULL)
		return -1; /* An error occured */

	/* Skip whitespaces, empty and comment lines */
	for (ptr = line; isspace(*ptr); ptr++);
	if (*ptr == '\0' || *ptr == '#')
		goto skip;
	
	return sample_io_villas_scan(line, s, fl);
}

#ifdef WITH_JANSSON
int sample_io_json_pack(json_t **j, struct sample *s, int flags)
{
	json_error_t err;
	json_t *json_data = json_array();
	
	for (int i = 0; i < s->length; i++) {
		json_t *json_value = sample_get_data_format(s, i)
					? json_integer(s->data[i].i)
					: json_real(s->data[i].f);
						
		json_array_append(json_data, json_value);
	}
	
	*j = json_pack_ex(&err, 0, "{ s: { s: [ I, I ], s: [ I, I ], s: [ I, I ] }, s: I, s: o }",
		"ts",
			"origin", s->ts.origin.tv_sec, s->ts.origin.tv_nsec,
			"received", s->ts.received.tv_sec, s->ts.received.tv_nsec,
			"sent", s->ts.sent.tv_sec, s->ts.sent.tv_nsec,
		"sequence", s->sequence,
		"data", json_data);
		
	if (!*j)
		return -1;
	
	return 0;
}

int sample_io_json_unpack(json_t *j, struct sample *s, int *flags)
{
	int ret, i;
	json_t *json_data, *json_value;
	
	ret = json_unpack(j, "{ s: { s: [ I, I ], s: [ I, I ], s: [ I, I ] }, s: I, s: o }",
		"ts",
			"origin", &s->ts.origin.tv_sec, &s->ts.origin.tv_nsec,
			"received", &s->ts.received.tv_sec, &s->ts.received.tv_nsec,
			"sent", &s->ts.sent.tv_sec, &s->ts.sent.tv_nsec,
		"sequence", &s->sequence,
		"data", &json_data);
		
	if (ret)
		return ret;
	
	s->length = 0;

	json_array_foreach(json_data, i, json_value) {
		switch (json_typeof(json_value)) {
			case JSON_REAL:
				s->data[i].f = json_real_value(json_value);
				sample_set_data_format(s, i, SAMPLE_DATA_FORMAT_FLOAT);
				break;

			case JSON_INTEGER:
				s->data[i].f = json_integer_value(json_value);
				sample_set_data_format(s, i, SAMPLE_DATA_FORMAT_INT);
				break;

			default:
				return -1;
		}
		
		s->length++;
	}

	return 0;
}

int sample_io_json_fprint(FILE *f, struct sample *s, int flags)
{
	int ret;
	json_t *json;
	
	ret = sample_io_json_pack(&json, s, flags);
	if (ret)
		return ret;
	
	ret = json_dumpf(json, f, 0);
	
	json_decref(json);
	
	return ret;
}

int sample_io_json_fscan(FILE *f, struct sample *s, int *flags)
{
	int ret;
	json_t *json;
	json_error_t err;

	json = json_loadf(f, JSON_DISABLE_EOF_CHECK, &err);
	if (!json)
		return -1;
	
	ret = sample_io_json_unpack(json, s, flags);
	
	json_decref(json);
	
	return ret;
}
#endif