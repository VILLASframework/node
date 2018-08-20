/** The internal datastructure for a sample of simulation data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdbool.h>
#include <inttypes.h>
#include <string.h>

#include <villas/io.h>
#include <villas/plugin.h>
#include <villas/utils.h>
#include <villas/timing.h>
#include <villas/sample.h>
#include <villas/signal.h>
#include <villas/formats/villas_human.h>

static size_t villas_human_sprint_single(struct io *io, char *buf, size_t len, const struct sample *smp)
{
	size_t off = 0;
	struct signal *sig;

	if (io->flags & SAMPLE_HAS_TS_ORIGIN) {
		off += snprintf(buf + off, len - off, "%llu", (unsigned long long) smp->ts.origin.tv_sec);
		off += snprintf(buf + off, len - off, ".%09llu", (unsigned long long) smp->ts.origin.tv_nsec);
	}

	if (io->flags & SAMPLE_HAS_TS_RECEIVED)
		off += snprintf(buf + off, len - off, "%+e", time_delta(&smp->ts.origin, &smp->ts.received));

	if (io->flags & SAMPLE_HAS_SEQUENCE)
		off += snprintf(buf + off, len - off, "(%" PRIu64 ")", smp->sequence);

	if (io->flags & SAMPLE_HAS_DATA) {
		for (int i = 0; i < smp->length; i++) {
			sig = list_at_safe(smp->signals, i);
			if (!sig)
				break;

			off += snprintf(buf + off, len - off, "%c", io->separator);
			off += signal_data_snprint(&smp->data[i], sig, buf + off, len - off);
		}
	}

	off += snprintf(buf + off, len - off, "%c", io->delimiter);

	return off;
}

static size_t villas_human_sscan_single(struct io *io, const char *buf, size_t len, struct sample *smp)
{
	int ret;
	char *end, *next;
	const char *ptr = buf;

	double offset = 0;

	smp->flags = 0;
	smp->signals = io->signals;

	/* Format: Seconds.NanoSeconds+Offset(SequenceNumber) Value1 Value2 ...
	 * RegEx: (\d+(?:\.\d+)?)([-+]\d+(?:\.\d+)?(?:e[+-]?\d+)?)?(?:\((\d+)\))?
	 *
	 * Please note that only the seconds and at least one value are mandatory
	 */

	/* Mandatory: seconds */
	smp->ts.origin.tv_sec = (uint32_t) strtoul(ptr, &end, 10);
	if (ptr == end || *end == io->delimiter)
		return -1;

	smp->flags |= SAMPLE_HAS_TS_ORIGIN;

	/* Optional: nano seconds */
	if (*end == '.') {
		ptr = end + 1;

		smp->ts.origin.tv_nsec = (uint32_t) strtoul(ptr, &end, 10);
		if (ptr == end)
			return -3;
	}
	else
		smp->ts.origin.tv_nsec = 0;

	/* Optional: offset / delay */
	if (*end == '+' || *end == '-') {
		ptr = end;

		offset = strtof(ptr, &end); /* offset is ignored for now */
		if (ptr != end)
			smp->flags |= SAMPLE_HAS_OFFSET;
		else
			return -4;
	}

	/* Optional: sequence */
	if (*end == '(') {
		ptr = end + 1;

		smp->sequence = strtoul(ptr, &end, 10);
		if (ptr != end)
			smp->flags |= SAMPLE_HAS_SEQUENCE;
		else
			return -5;

		if (*end == ')')
			end++;
	}

	int i;
	for (ptr = end + 1, i = 0; i < smp->capacity; ptr = end + 1, i++) {

		if (*end == io->delimiter)
			goto out;

		struct signal *sig = (struct signal *) list_at_safe(io->signals, i);
		if (!sig)
			goto out;

		/* Perform signal detection only once */
		if (sig->type == SIGNAL_TYPE_AUTO) {

			/* Find end of the current column */
			next = strpbrk(ptr, (char[]) { io->separator, io->delimiter, 0 });
			if (next == NULL)
				goto out;

			/* Copy value to temporary '\0' terminated buffer */
			size_t len = next - ptr;
			char val[len+1];
			strncpy(val, ptr, len);
			val[len] = '\0';

			sig->type = signal_type_detect(val);

			debug(LOG_IO | 5, "Learned data type for index %u: %s", i, signal_type_to_str(sig->type));
		}

		ret = signal_data_parse_str(&smp->data[i], sig, ptr, &end);
		if (ret || end == ptr) /* There are no valid values anymore. */
			goto out;
	}

out:	if (*end == io->delimiter)
		end++;

	smp->length = i;
	if (smp->length > 0)
		smp->flags |= SAMPLE_HAS_DATA;

	if (smp->flags & SAMPLE_HAS_OFFSET) {
		struct timespec off = time_from_double(offset);
		smp->ts.received = time_add(&smp->ts.origin, &off);

		smp->flags |= SAMPLE_HAS_TS_RECEIVED;
	}

	return end - buf;
}

int villas_human_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	int i;
	size_t off = 0;

	for (i = 0; i < cnt && off < len; i++)
		off += villas_human_sprint_single(io, buf + off, len - off, smps[i]);

	if (wbytes)
		*wbytes = off;

	return i;
}

int villas_human_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	int i;
	size_t off = 0;

	for (i = 0; i < cnt && off < len; i++)
		off += villas_human_sscan_single(io, buf + off, len - off, smps[i]);

	if (rbytes)
		*rbytes = off;

	return i;
}

void villas_human_header(struct io *io, const struct sample *smp)
{
	FILE *f = io_stream_output(io);

	fprintf(f, "# %-20s", "seconds.nanoseconds+offset(sequence)");

	for (int i = 0; i < smp->length; i++) {
		struct signal *sig = (struct signal *) list_at(smp->signals, i);

		if (sig->name)
			fprintf(f, "%c%s", io->separator, sig->name);
		else
			fprintf(f, "%csignal%d", io->separator, i);

		if (sig->unit)
			fprintf(f, "[%s]", sig->unit);
	}

	fprintf(f, "%c", io->delimiter);
}

static struct plugin p = {
	.name = "villas.human",
	.description = "VILLAS human readable format",
	.type = PLUGIN_TYPE_FORMAT,
	.format = {
		.sprint	= villas_human_sprint,
		.sscan	= villas_human_sscan,
		.header = villas_human_header,
		.size	= 0,
		.flags	= IO_NEWLINES | IO_AUTO_DETECT_FORMAT |
		          SAMPLE_HAS_TS_ORIGIN | SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_DATA,
		.separator = '\t',
		.delimiter = '\n'
	}
};

REGISTER_PLUGIN(&p);
