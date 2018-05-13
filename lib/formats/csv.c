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

#include <villas/io.h>
#include <villas/formats/csv.h>
#include <villas/plugin.h>
#include <villas/sample.h>
#include <villas/signal.h>
#include <villas/timing.h>

static size_t csv_sprint_single(struct io *io, char *buf, size_t len, struct sample *s)
{
	size_t off = 0;

	if (io->flags & SAMPLE_HAS_ORIGIN)
		off += snprintf(buf + off, len - off, "%ld%c%09ld", s->ts.origin.tv_sec, CSV_SEPARATOR, s->ts.origin.tv_nsec);
	else
		off += snprintf(buf + off, len - off, "nan%cnan", CSV_SEPARATOR);

	if (io->flags & SAMPLE_HAS_RECEIVED)
		off += snprintf(buf + off, len - off, "%c%f", CSV_SEPARATOR, time_delta(&s->ts.origin, &s->ts.received));
	else
		off += snprintf(buf + off, len - off, "%cnan", CSV_SEPARATOR);

	if (io->flags & SAMPLE_HAS_SEQUENCE)
		off += snprintf(buf + off, len - off, "%c%u", CSV_SEPARATOR, s->sequence);
	else
		off += snprintf(buf + off, len - off, "%cnan", CSV_SEPARATOR);

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

static size_t csv_sscan_single(struct io *io, const char *buf, size_t len, struct sample *s)
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

	double offset __attribute__((unused)) = strtof(ptr, &end);
	if (end == ptr || *end == '\n')
		goto out;

	ptr = end;

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

int csv_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	int i;
	size_t off = 0;

	for (i = 0; i < cnt && off < len; i++)
		off += csv_sprint_single(io, buf + off, len - off, smps[i]);

	if (wbytes)
		*wbytes = off;

	return i;
}

int csv_sscan(struct io *io, char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	int i;
	size_t off = 0;

	for (i = 0; i < cnt && off < len; i++)
		off += csv_sscan_single(io, buf + off, len - off, smps[i]);

	if (rbytes)
		*rbytes = off;

	return i;
}

void csv_header(struct io *io)
{
	FILE *f = io_stream_output(io);

	fprintf(f, "# secs%cnsecs%coffset%csequence", CSV_SEPARATOR, CSV_SEPARATOR, CSV_SEPARATOR);

	if (io->output.signals) {
		for (int i = 0; i < list_length(io->output.signals); i++) {
			struct signal *s = (struct signal *) list_at(io->output.signals, i);

			fprintf(f, "%c%s", CSV_SEPARATOR, s->name);

			if (s->unit)
				fprintf(f, "[%s]", s->unit);
		}
	}
	else
		fprintf(f, "%cdata[]", CSV_SEPARATOR);

	fprintf(f, "\n");
}

static struct plugin p = {
	.name = "csv",
	.description = "Tabulator-separated values",
	.type = PLUGIN_TYPE_FORMAT,
	.io = {
		.sprint	= csv_sprint,
		.sscan	= csv_sscan,
		.header	= csv_header,
		.size 	= 0,
		.flags	= IO_NEWLINES
	}
};

REGISTER_PLUGIN(&p);
