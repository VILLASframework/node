/** The internal datastructure for a sample of simulation data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <cinttypes>
#include <cstring>

#include <villas/utils.hpp>
#include <villas/timing.h>
#include <villas/sample.h>
#include <villas/signal.h>
#include <villas/formats/villas_human.hpp>

using namespace villas::node;

size_t VILLASHumanFormat::sprintLine(char *buf, size_t len, const struct sample *smp)
{
	size_t off = 0;
	struct signal *sig;

	if (flags & (int) SampleFlags::HAS_TS_ORIGIN) {
		if (smp->flags & (int) SampleFlags::HAS_TS_ORIGIN) {
			off += snprintf(buf + off, len - off, "%llu", (unsigned long long) smp->ts.origin.tv_sec);
			off += snprintf(buf + off, len - off, ".%09llu", (unsigned long long) smp->ts.origin.tv_nsec);
		}
		else
			off += snprintf(buf + off, len - off, "nan.nan");
	}

	if (flags & (int) SampleFlags::HAS_OFFSET) {
		if (smp->flags & (int) SampleFlags::HAS_TS_RECEIVED)
			off += snprintf(buf + off, len - off, "%+e", time_delta(&smp->ts.origin, &smp->ts.received));
	}

	if (flags & (int) SampleFlags::HAS_SEQUENCE) {
		if (smp->flags & (int) SampleFlags::HAS_SEQUENCE)
			off += snprintf(buf + off, len - off, "(%" PRIu64 ")", smp->sequence);
	}

	if (flags & (int) SampleFlags::HAS_DATA) {
		for (unsigned i = 0; i < smp->length; i++) {
			sig = (struct signal *) vlist_at_safe(smp->signals, i);
			if (!sig)
				break;

			off += snprintf(buf + off, len - off, "\t");
			off += signal_data_print_str(&smp->data[i], sig->type, buf + off, len - off, real_precision);
		}
	}

	off += snprintf(buf + off, len - off, "%c", delimiter);

	return off;
}

size_t VILLASHumanFormat::sscanLine(const char *buf, size_t len, struct sample *smp)
{
	int ret;
	char *end;
	const char *ptr = buf;

	double offset = 0;

	smp->flags = 0;
	smp->signals = signals;

	/* Format: Seconds.NanoSeconds+Offset(SequenceNumber) Value1 Value2 ...
	 * RegEx: (\d+(?:\.\d+)?)([-+]\d+(?:\.\d+)?(?:e[+-]?\d+)?)?(?:\((\d+)\))?
	 *
	 * Please note that only the seconds and at least one value are mandatory
	 */

	/* Mandatory: seconds */
	smp->ts.origin.tv_sec = (uint32_t) strtoul(ptr, &end, 10);
	if (ptr == end || *end == delimiter)
		return -1;

	smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;

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
			smp->flags |= (int) SampleFlags::HAS_OFFSET;
		else
			return -4;
	}

	/* Optional: sequence */
	if (*end == '(') {
		ptr = end + 1;

		smp->sequence = strtoul(ptr, &end, 10);
		if (ptr != end)
			smp->flags |= (int) SampleFlags::HAS_SEQUENCE;
		else
			return -5;

		if (*end == ')')
			end++;
	}

	unsigned i;
	for (ptr = end + 1, i = 0; i < smp->capacity; ptr = end + 1, i++) {
		if (*end == delimiter)
			goto out;

		struct signal *sig = (struct signal *) vlist_at_safe(signals, i);
		if (!sig)
			goto out;

		ret = signal_data_parse_str(&smp->data[i], sig->type, ptr, &end);
		if (ret || end == ptr) /* There are no valid values anymore. */
			goto out;
	}

out:	if (*end == delimiter)
		end++;

	smp->length = i;
	if (smp->length > 0)
		smp->flags |= (int) SampleFlags::HAS_DATA;

	if (smp->flags & (int) SampleFlags::HAS_OFFSET) {
		struct timespec off = time_from_double(offset);
		smp->ts.received = time_add(&smp->ts.origin, &off);

		smp->flags |= (int) SampleFlags::HAS_TS_RECEIVED;
	}

	return end - buf;
}

void VILLASHumanFormat::header(FILE *f, const struct vlist *sigs)
{
	/* Abort if we are not supposed to, or have already printed the header */
	if (!print_header || header_printed)
		return;

	fprintf(f, "# ");

	if (flags & (int) SampleFlags::HAS_TS_ORIGIN)
		fprintf(f, "seconds.nanoseconds");

	if (flags & (int) SampleFlags::HAS_OFFSET)
		fprintf(f, "+offset");

	if (flags & (int) SampleFlags::HAS_SEQUENCE)
		fprintf(f, "(sequence)");

	if (flags & (int) SampleFlags::HAS_DATA) {
		for (unsigned i = 0; i < vlist_length(sigs); i++) {
			struct signal *sig = (struct signal *) vlist_at_safe(sigs, i);
			if (!sig)
				break;

			if (sig->name)
				fprintf(f, "\t%s", sig->name);
			else
				fprintf(f, "\tsignal%u", i);

			if (sig->unit)
				fprintf(f, "[%s]", sig->unit);
		}
	}

	fprintf(f, "%c", delimiter);

	LineFormat::header(f, sigs);
}

static char n[] = "villas.human";
static char d[] = "VILLAS human readable format";
static LineFormatPlugin<VILLASHumanFormat, n, d, (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA, '\n'> p;
