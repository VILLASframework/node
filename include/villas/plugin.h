/** Loadable / plugin support.
 *
 * @file
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

#pragma once

#include <villas/common.h>
#include <villas/utils.hpp>
#include <villas/node_type.h>
#include <villas/format_type.h>

extern struct vlist plugins;

enum plugin_type {
	PLUGIN_TYPE_NODE,
	PLUGIN_TYPE_FORMAT,
};

struct plugin {
	const char *name;
	const char *description;

	enum plugin_type type;

	union {
		struct format_type	format;
		struct node_type	node;
	};
};

/** Return a pointer to the plugin structure */
#define plugin(vt) ((struct plugin *) ((char *) (vt) - offsetof(struct plugin, format)))

#define plugin_name(vt) plugin(vt)->name
#define plugin_description(vt) plugin(vt)->description

void plugin_dump(enum plugin_type type);

/** Find registered and loaded plugin with given name and type. */
struct plugin * plugin_lookup(enum plugin_type type, const char *name);
