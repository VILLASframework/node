/** Bare text values.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/formats/value.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>

using namespace villas::node;

int ValueFormat::sprint(char *buf, size_t len, size_t *wbytes, const struct Sample * const smps[], unsigned cnt)
{
	unsigned i;
	size_t off = 0;
	const struct Sample *smp = smps[0];

	assert(cnt == 1);
	assert(smp->length <= 1);

	buf[0] = '\0';

	for (i = 0; i < smp->length; i++) {
		auto sig = smp->signals->getByIndex(i);
		if (!sig)
			return -1;

		off += smp->data[i].printString(sig->type, buf, len, real_precision);
		off += snprintf(buf + off, len - off, "\n");
	}

	if (wbytes)
		*wbytes = off;

	return i;
}

int ValueFormat::sscan(const char *buf, size_t len, size_t *rbytes, struct Sample * const smps[], unsigned cnt)
{
	unsigned i = 0, ret;
	struct Sample *smp = smps[0];

	const char *ptr = buf;
	char *end;

	assert(cnt == 1);

	printf("Reading: %s", buf);

	if (smp->capacity >= 1) {
		auto sig = signals->getByIndex(i);
		if (!sig)
			return -1;

		ret = smp->data[i].parseString(sig->type, ptr, &end);
		if (ret || end == ptr) /* There are no valid values anymore. */
			goto out;

		i++;
		ptr = end;
	}

out:	smp->flags = 0;
	smp->signals = signals;
	smp->length = i;
	if (smp->length > 0)
		smp->flags |= (int) SampleFlags::HAS_DATA;

	if (rbytes)
		*rbytes = ptr - buf;

	return i;
}

static char n[] = "value";
static char d[] = "A bare text value without any headers";
static FormatPlugin<ValueFormat, n, d, (int) SampleFlags::HAS_DATA> p;
