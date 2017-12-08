/** Comma-separated values.
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

#include <ctype.h>
#include <inttypes.h>
#include <string.h>

#include <villas/io/csv.h>
#include <villas/plugin.h>
#include <villas/sample.h>
#include <villas/timing.h>

size_t csv_sprint_single(char *buf, size_t len, struct sample *s, int flags)
{
	size_t off = 0;

	if (flags & SAMPLE_HAS_ORIGIN)
		off += snprintf(buf + off, len - off, "%ld%c%09ld", s->ts.origin.tv_sec, CSV_SEPARATOR, s->ts.origin.tv_nsec);

	if (flags & SAMPLE_HAS_SEQUENCE)
		off += snprintf(buf + off, len - off, "%c%u", CSV_SEPARATOR, s->sequence);

	for (int i = 0; i < s->length; i++) {
		switch ((s->format >> i) & 0x1) {
			case SAMPLE_DATA_FORMAT_FLOAT:
				off += snprintf(buf + off, len - off, "%c%.6f", CSV_SEPARATOR, s->data[i].f);
				break;
			case SAMPLE_DATA_FORMAT_INT:
				off += snprintf(buf + off, len - off, "%c%" PRId64, CSV_SEPARATOR, s->data[i].i);
				break;
		}
	}

	off += snprintf(buf + off, len - off, "\n");

	return off;
}

size_t csv_sscan_single(const char *buf, size_t len, struct sample *s, int flags)
{
	const char *ptr = buf;
	char *end;

	s->flags = 0;

	s->ts.origin.tv_sec = strtoul(ptr, &end, 10);
	if (end == ptr || *end == '\n')
		goto out;

	ptr = end;

	s->ts.origin.tv_nsec = strtoul(ptr, &end, 10);
	if (end == ptr || *end == '\n')
		goto out;

	ptr = end;

	s->flags |= SAMPLE_HAS_ORIGIN;

	s->sequence = strtoul(ptr, &end, 10);
	if (end == ptr || *end == '\n')
		goto out;

	s->flags |= SAMPLE_HAS_SEQUENCE;

	for (ptr  = end, s->length = 0;
	                 s->length < s->capacity;
	     ptr  = end, s->length++) {
		if (*end == '\n')
			goto out;

		switch (s->format & (1 << s->length)) {
			case SAMPLE_DATA_FORMAT_FLOAT:
				s->data[s->length].f = strtod(ptr, &end);
				break;
			case SAMPLE_DATA_FORMAT_INT:
				s->data[s->length].i = strtol(ptr, &end, 10);
				break;
		}

		/* There are no valid FP values anymore. */
		if (end == ptr)
			goto out;
	}

out:	if (*end == '\n')
		end++;

	if (s->length > 0)
		s->flags |= SAMPLE_HAS_VALUES;

	return end - buf;
}

int csv_sprint(char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt, int flags)
{
	int i;
	size_t off = 0;

	for (i = 0; i < cnt && off < len; i++)
		off += csv_sprint_single(buf + off, len - off, smps[i], flags);

	if (wbytes)
		*wbytes = off;

	return i;
}

int csv_sscan(char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt, int flags)
{
	int i;
	size_t off = 0;

	for (i = 0; i < cnt && off < len; i++)
		off += csv_sscan_single(buf + off, len - off, smps[i], flags);

	if (rbytes)
		*rbytes = off;

	return i;
}

int csv_fprint_single(FILE *f, struct sample *s, int flags)
{
	int ret;
	char line[4096];

	ret = csv_sprint_single(line, sizeof(line), s, flags);
	if (ret < 0)
		return ret;

	fputs(line, f);

	return 0;
}

int csv_fscan_single(FILE *f, struct sample *s, int flags)
{
	char *ptr, line[4096];

skip:	if (fgets(line, sizeof(line), f) == NULL)
		return -1; /* An error occured */

	/* Skip whitespaces, empty and comment lines */
	for (ptr = line; isspace(*ptr); ptr++);
	if (*ptr == '\0' || *ptr == '#')
		goto skip;

	return csv_sscan_single(line, strlen(line), s, flags);
}

int csv_fprint(FILE *f, struct sample *smps[], unsigned cnt, int flags)
{
	int ret, i;
	for (i = 0; i < cnt; i++) {
		ret = csv_fprint_single(f, smps[i], flags);
		if (ret < 0)
			break;
	}

	return i;
}

int csv_fscan(FILE *f, struct sample *smps[], unsigned cnt, int flags)
{
	int ret, i;
	for (i = 0; i < cnt; i++) {
		ret = csv_fscan_single(f, smps[i], flags);
		if (ret < 0) {
			warn("Failed to read CSV line: %d", ret);
			break;
		}
	}

	return i;
}

static struct plugin p = {
	.name = "csv",
	.description = "Tabulator-separated values",
	.type = PLUGIN_TYPE_IO,
	.io = {
		.fprint	= csv_fprint,
		.fscan	= csv_fscan,
		.sprint	= csv_sprint,
		.sscan	= csv_sscan,
		.size = 0
	}
};

REGISTER_PLUGIN(&p);
