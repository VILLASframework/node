/** Bare text values.
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

#include <villas/io.h>
#include <villas/formats/csv.h>
#include <villas/plugin.h>
#include <villas/sample.h>
#include <villas/signal.h>

using namespace villas::utils;

int value_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	unsigned i;
	size_t off = 0;
	struct signal *sig;
	struct sample *smp = smps[0];

	assert(cnt == 1);
	assert(smp->length <= 1);

	buf[0] = '\0';

	for (i = 0; i < smp->length; i++) {
		sig = (struct signal *) vlist_at_safe(smp->signals, i);
		if (!sig)
			break;

		off += signal_data_print_str(&smp->data[i], sig, buf, len);
		off += snprintf(buf + off, len - off, "\n");
	}

	if (wbytes)
		*wbytes = off;

	return i;
}

int value_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	unsigned i = 0, ret;
	struct sample *smp = smps[0];

	const char *ptr = buf;
	char *end;

	assert(cnt == 1);

	printf("Reading: %s", buf);

	if (smp->capacity >= 1) {
		struct signal *sig = (struct signal *) vlist_at_safe(io->signals, i);
		if (!sig)
			goto out;

		ret = signal_data_parse_str(&smp->data[i], sig, ptr, &end);
		if (ret || end == ptr) /* There are no valid values anymore. */
			goto out;

		i++;
		ptr = end;
	}

out:	smp->flags = 0;
	smp->signals = io->signals;
	smp->length = i;
	if (smp->length > 0)
		smp->flags |= (int) SampleFlags::HAS_DATA;

	if (rbytes)
		*rbytes = ptr - buf;

	return i;
}

static struct plugin p;
__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
	if (plugins.state == State::DESTROYED)
		vlist_init(&plugins);

	p.name = "value";
	p.description = "A bare text value without any headers";
	p.type = PluginType::FORMAT;
	p.format.sprint = value_sprint;
	p.format.sscan	= value_sscan;
	p.format.size 	= 0;
	p.format.flags	= (int) SampleFlags::HAS_DATA | (int) IOFlags::NEWLINES;

	vlist_push(&plugins, &p);
}

__attribute__((destructor(110))) static void UNIQUE(__dtor)() {
	if (plugins.state != State::DESTROYED)
		vlist_remove_all(&plugins, &p);
}
