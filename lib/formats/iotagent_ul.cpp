/** UltraLight format for FISMEP project.
 *
 * @author Iris Koester <ikoester@eonerc.rwth-aachen.de>
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

#include <cstring>

#include <villas/plugin.h>
#include <villas/sample.h>
#include <villas/node.h>
#include <villas/signal.h>
#include <villas/compat.h>
#include <villas/timing.h>
#include <villas/io.h>
#include <villas/formats/json.h>

int iotagent_ul_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	size_t printed = 0;
	struct signal *sig;
	struct sample *smp = smps[0];

	for (unsigned i = 0; (i < smp->length) && (printed < len); i++) {
		sig = (struct signal *) vlist_at_safe(smp->signals, i);
		if (!sig)
			return -1;

		if (sig->name)
			printed += snprintf(buf + printed, len - printed, "%s|%f|", sig->name, smp->data[i].f);
		else {
			char name[32];
			snprintf(name, 32, "signal_%d", i);
			printed += snprintf(buf + printed, len - printed, "%s|%f|", name, smp->data[i].f);
		}
	}

	if (wbytes)
		*wbytes = printed - 1; // -1 to cut off last '|'

	return 0;
}

static struct plugin p;

__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
	if (plugins.state == State::DESTROYED)
		vlist_init(&plugins);

	p.name = "iotagent_ul";
	p.description = "FIWARE IotAgent UltraLight format";
	p.type = PluginType::FORMAT;
	p.format.sprint	= iotagent_ul_sprint;
	p.format.size = 0;

	vlist_push(&plugins, &p);
}

__attribute__((destructor(110))) static void UNIQUE(__dtor)() {
	if (plugins.state != State::DESTROYED)
		vlist_remove_all(&plugins, &p);
}
