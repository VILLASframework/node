/** Read / write sample data in different formats.
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

#include <cstdlib>
#include <cstdio>

#include <villas/plugin.h>
#include <villas/format_type.h>

struct format_type * format_type_lookup(const char *name)
{
	struct plugin *p;

	p = plugin_lookup(PluginType::FORMAT, name);
	if (!p)
		return nullptr;

	return &p->format;
}

const char * format_type_name(struct format_type *vt)
{
	return plugin_name(vt);
}
