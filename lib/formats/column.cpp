/** Comma-separated values.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <cctype>
#include <cinttypes>
#include <cstring>

#include <villas/formats/column.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>
#include <villas/timing.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;

size_t ColumnLineFormat::sprintLine(char *buf, size_t len, const struct Sample *smp)
{
	size_t off = 0;

	if (smp->flags & (int) SampleFlags::HAS_TS_ORIGIN)
			off += snprintf(buf + off, len - off, "%lld%c%09lld", (long long) smp->ts.origin.tv_sec, separator,
									      (long long) smp->ts.origin.tv_nsec);
	else
		off += snprintf(buf + off, len - off, "nan%cnan", separator);

	if (smp->flags & (int) SampleFlags::HAS_TS_RECEIVED)
		off += snprintf(buf + off, len - off, "%c%.09f", separator, time_delta(&smp->ts.origin, &smp->ts.received));
	else
		off += snprintf(buf + off, len - off, "%cnan", separator);

	if (smp->flags & (int) SampleFlags::HAS_SEQUENCE)
		off += snprintf(buf + off, len - off, "%c%" PRIu64, separator, smp->sequence);
	else
		off += snprintf(buf + off, len - off, "%cnan", separator);

	for (unsigned i = 0; i < smp->length; i++) {
		auto sig = smp->signals->getByIndex(i);
		if (!sig)
			break;

		off += snprintf(buf + off, len - off, "%c", separator);
		off += smp->data[i].printString(sig->type, buf + off, len - off, real_precision);
	}

	off += snprintf(buf + off, len - off, "%c", delimiter);

	return off;
}

size_t ColumnLineFormat::sscanLine(const char *buf, size_t len, struct Sample *smp)
{
	int ret;
	unsigned i = 0;
	const char *ptr = buf;
	char *end;

	struct timespec offset;

	smp->flags = 0;
	smp->signals = signals;

	smp->ts.origin.tv_sec = strtoul(ptr, &end, 10);
	if (end == ptr || *end == delimiter)
		goto out;

	ptr = end + 1;

	smp->ts.origin.tv_nsec = strtoul(ptr, &end, 10);
	if (end == ptr || *end == delimiter)
		goto out;

	ptr = end + 1;

	smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;

	offset = time_from_double(strtof(ptr, &end));
	if (end == ptr || *end == delimiter)
		goto out;

	smp->ts.received = time_add(&smp->ts.origin, &offset);

	smp->flags |= (int) SampleFlags::HAS_TS_RECEIVED;

	ptr = end + 1;

	smp->sequence = strtoul(ptr, &end, 10);
	if (end == ptr || *end == delimiter)
		goto out;

	smp->flags |= (int) SampleFlags::HAS_SEQUENCE;

	for (ptr = end + 1, i = 0; i < smp->capacity; ptr = end + 1, i++) {
		if (*end == delimiter)
			goto out;

		auto sig = smp->signals->getByIndex(i);
		if (!sig)
			goto out;

		ret = smp->data[i].parseString(sig->type, ptr, &end);
		if (ret || end == ptr) /* There are no valid values anymore. */
			goto out;
	}

out:	if (*end == delimiter)
		end++;

	smp->length = i;
	if (smp->length > 0)
		smp->flags |= (int) SampleFlags::HAS_DATA;

	return end - buf;
}

void ColumnLineFormat::header(FILE *f, const SignalList::Ptr sigs)
{
	/* Abort if we are not supposed to, or have already printed the header */
	if (!print_header || header_printed)
		return;

	if (comment)
		fprintf(f, "%c", comment);

	if (flags & (int) SampleFlags::HAS_TS_ORIGIN)
		fprintf(f, "secs%cnsecs%c", separator, separator);

	if (flags & (int) SampleFlags::HAS_OFFSET)
		fprintf(f, "offset%c", separator);

	if (flags & (int) SampleFlags::HAS_SEQUENCE)
		fprintf(f, "sequence%c", separator);

	if (flags & (int) SampleFlags::HAS_DATA) {
		for (unsigned i = 0; i < sigs->size(); i++) {
			auto sig = sigs->getByIndex(i);
			if (!sig)
				break;

			if (!sig->name.empty())
				fprintf(f, "%s", sig->name.c_str());
			else
				fprintf(f, "signal%u", i);

			if (!sig->unit.empty())
				fprintf(f, "[%s]", sig->unit.c_str());

			if (i + 1 < sigs->size())
				fprintf(f, "%c", separator);
		}
	}

	fprintf(f, "%c", delimiter);

	LineFormat::header(f, sigs);
}

void ColumnLineFormat::parse(json_t *json)
{
	int ret;
	json_error_t err;
	const char *sep = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s?: s }",
		"separator", &sep
	);
	if (ret)
		throw ConfigError(json, err, "node-config-format-column", "Failed to parse format configuration");

	if (sep) {
		if (strlen(sep) != 1)
			throw ConfigError(json, "node-config-format-column-separator", "Column separator must be a single character!");

		separator = sep[0];
	}

	LineFormat::parse(json);
}


static char n1[] = "csv";
static char d1[] = "Comma-separated values";
static ColumnLineFormatPlugin<n1, d1, (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA, '\n', ','> p1;

static char n2[] = "tsv";
static char d2[] = "Tabulator-separated values";
static ColumnLineFormatPlugin<n2, d2, (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA, '\n', '\t'> p2;
