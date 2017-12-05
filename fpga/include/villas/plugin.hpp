/** Loadable / plugin support.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
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

#include <list>
#include <string>
#include <jansson.h>

#include "utils.h"

namespace villas {

class Plugin {
public:

	enum class Type {
		Unknown,
		FpgaIp,
	};

	Plugin();
	virtual ~Plugin();

	// each plugin is a singleton, so copying is not allowed
	Plugin(Plugin const&)  = delete;
	void operator=(Plugin const&) = delete;

	int load();
	int unload();

	virtual int parse(json_t *cfg);
	virtual void dump();

	/** Find registered and loaded plugin with given name and type. */
	static Plugin *
	lookup(Type type, std::string name);

	static std::list<Plugin*>
	lookup(Type type);

	// check if this makes sense! (no intermediate plugins)
	bool
	operator==(const Plugin& other) const;

	Type pluginType;

	std::string name;
	std::string description;
	std::string path;
	void *handle;

	enum state state;

private:
	using PluginList = std::list<Plugin *>;
	static std::list<Plugin *> pluginList;
};

} // namespace villas
