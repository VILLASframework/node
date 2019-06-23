/** Loadable / plugin support.
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

#include <dlfcn.h>
#include <cstring>

#include <villas/plugin.h>

/** Global list of all known plugins */
struct vlist plugins = { .state = State::DESTROYED };

LIST_INIT_STATIC(&plugins)

struct plugin * plugin_lookup(enum PluginType type, const char *name)
{
	for (size_t i = 0; i < vlist_length(&plugins); i++) {
		struct plugin *p = (struct plugin *) vlist_at(&plugins, i);

		if (p->type == type && strcmp(p->name, name) == 0)
			return p;
	}

	return nullptr;
}

void plugin_dump(enum PluginType type)
{
	for (size_t i = 0; i < vlist_length(&plugins); i++) {
		struct plugin *p = (struct plugin *) vlist_at(&plugins, i);

		if (p->type == type)
			printf(" - %-13s: %s\n", p->name, p->description);
	}
}
