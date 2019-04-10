/** Comma-separated values.
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

#include <ctype.h>
#include <inttypes.h>
#include <string.h>

#include <villas/io.h>
#include <villas/formats/csv.h>
#include <villas/plugin.h>
#include <villas/sample.h>
#include <villas/signal.h>
#include <villas/timing.h>

static size_t csv_sprint_single(struct io *io, char *buf, size_t len, const struct sample *smp)
{
	size_t off = 0;
	struct signal *sig;

	if (io->flags & SAMPLE_HAS_TS_ORIGIN) {
		if (io->flags & SAMPLE_HAS_TS_ORIGIN)
			off += snprintf(buf + off, len - off, "%ld%c%09ld", smp->ts.origin.tv_sec, io->separator, smp->ts.origin.tv_nsec);
		else
			off += snprintf(buf + off, len - off, "nan%cnan", io->separator);
	}

	if (io->flags & SAMPLE_HAS_OFFSET) {
		if (smp->flags & SAMPLE_HAS_TS_RECEIVED)
			off += snprintf(buf + off, len - off, "%c%.09f", io->separator, time_delta(&smp->ts.origin, &smp->ts.received));
		else
			off += snprintf(buf + off, len - off, "%cnan", io->separator);
	}

	if (io->flags & SAMPLE_HAS_SEQUENCE) {
		if (smp->flags & SAMPLE_HAS_SEQUENCE)
			off += snprintf(buf + off, len - off, "%c%" PRIu64, io->separator, smp->sequence);
		else
			off += snprintf(buf + off, len - off, "%cnan", io->separator);
	}

	if (io->flags & SAMPLE_HAS_DATA) {
		for (unsigned i = 0; i < smp->length; i++) {
			sig = (struct signal *) vlist_at_safe(smp->signals, i);
			if (!sig)
				break;

			off += snprintf(buf + off, len - off, "%c", io->separator);
			off += signal_data_snprint(&smp->data[i], sig, buf + off, len - off);
		}
	}

	off += snprintf(buf + off, len - off, "%c", io->delimiter);

	return off;
}

static size_t csv_sscan_single(struct io *io, const char *buf, size_t len, struct sample *smp)
{
	int ret;
	unsigned i = 0;
	const char *ptr = buf;
	char *end;

	double offset __attribute__((unused));

	smp->flags = 0;
	smp->signals = io->signals;

	smp->ts.origin.tv_sec = strtoul(ptr, &end, 10);
	if (end == ptr || *end == io->delimiter)
		goto out;

	ptr = end + 1;

	smp->ts.origin.tv_nsec = strtoul(ptr, &end, 10);
	if (end == ptr || *end == io->delimiter)
		goto out;

	ptr = end + 1;

	smp->flags |= SAMPLE_HAS_TS_ORIGIN;

	offset = strtof(ptr, &end);
	if (end == ptr || *end == io->delimiter)
		goto out;

	ptr = end + 1;

	smp->sequence = strtoul(ptr, &end, 10);
	if (end == ptr || *end == io->delimiter)
		goto out;

	smp->flags |= SAMPLE_HAS_SEQUENCE;

	for (ptr = end + 1, i = 0; i < smp->capacity; ptr = end + 1, i++) {

		if (*end == io->delimiter)
			goto out;

		struct signal *sig = (struct signal *) vlist_at_safe(smp->signals, i);
		if (!sig)
			goto out;

		ret = signal_data_parse_str(&smp->data[i], sig, ptr, &end);
		if (ret || end == ptr) /* There are no valid values anymore. */
			goto out;
	}

out:	if (*end == io->delimiter)
		end++;

	smp->length = i;
	if (smp->length > 0)
		smp->flags |= SAMPLE_HAS_DATA;

	return end - buf;
}

int csv_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	unsigned i;
	size_t off = 0;

	for (i = 0; i < cnt && off < len; i++)
		off += csv_sprint_single(io, buf + off, len - off, smps[i]);

	if (wbytes)
		*wbytes = off;

	return i;
}

int csv_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	unsigned i;
	size_t off = 0;

	for (i = 0; i < cnt && off < len; i++)
		off += csv_sscan_single(io, buf + off, len - off, smps[i]);

	if (rbytes)
		*rbytes = off;

	return i;
}

void csv_header(struct io *io, const struct sample *smp)
{
	FILE *f = io_stream_output(io);

	fprintf(f, "# ");
	if (io->flags & SAMPLE_HAS_TS_ORIGIN)
		fprintf(f, "secs%cnsecs%c", io->separator, io->separator);

	if (io->flags & SAMPLE_HAS_OFFSET)
		fprintf(f, "offset%c", io->separator);

	if (io->flags & SAMPLE_HAS_SEQUENCE)
		fprintf(f, "sequence%c", io->separator);

	if (io->flags & SAMPLE_HAS_DATA) {
		for (unsigned i = 0; i < smp->length; i++) {
			struct signal *sig = (struct signal *) vlist_at(smp->signals, i);

			if (sig->name)
				fprintf(f, "%s", sig->name);
			else
				fprintf(f, "signal%d", i);

			if (sig->unit)
				fprintf(f, "[%s]", sig->unit);

			if (i + 1 < smp->length)
				fprintf(f, "%c", io->separator);
		}
	}

	fprintf(f, "%c", io->delimiter);
}

static struct plugin p1;
__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
        if (plugins.state == STATE_DESTROYED)
	                vlist_init(&plugins);

	p1.name = "tsv";
	p1.description = "Tabulator-separated values";
	p1.type = PLUGIN_TYPE_FORMAT;
	p1.format.header = csv_header;
	p1.format.sprint = csv_sprint;
	p1.format.sscan	= csv_sscan;
	p1.format.size 	= 0;
	p1.format.flags	= IO_NEWLINES |
		          SAMPLE_HAS_TS_ORIGIN | SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_DATA;
	p1.format.separator = '\t';

	vlist_push(&plugins, &p1);
}

__attribute__((destructor(110))) static void UNIQUE(__dtor)() {
        if (plugins.state != STATE_DESTROYED)
                vlist_remove_all(&plugins, &p1);
}

static struct plugin p2;
__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
        if (plugins.state == STATE_DESTROYED)
	                vlist_init(&plugins);

	p2.name = "csv";
	p2.description = "Comma-separated values";
	p2.type = PLUGIN_TYPE_FORMAT;
	p1.format.header = csv_header;
	p1.format.sprint = csv_sprint;
	p1.format.sscan	= csv_sscan;
	p1.format.size 	= 0;
	p1.format.flags	= IO_NEWLINES |
			  SAMPLE_HAS_TS_ORIGIN | SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_DATA;
	p1.format.separator = ',';

	vlist_push(&plugins, &p2);
}

__attribute__((destructor(110))) static void UNIQUE(__dtor)() {
        if (plugins.state != STATE_DESTROYED)
                vlist_remove_all(&plugins, &p2);
}
