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

#include "log.hpp"
#include "utils.h"

namespace villas {

class Plugin {
public:

	enum class Type {
		Unknown,
		FpgaIp,
		FpgaCard,
	};

	Plugin(Type type, const std::string& name);
	virtual ~Plugin();

	// copying a plugin doesn't make sense, so explicitly deny it
	Plugin(Plugin const&)  = delete;
	void operator=(Plugin const&) = delete;

	int load();
	int unload();

	virtual int parse(json_t *cfg);
	virtual void dump();

	static void
	dumpList();

	/// Find plugin by type and (optional if empty) by name. If more match, it
	/// is not defined which one will be returned.
	static Plugin *
	lookup(Type type, std::string name);

	/// Get all plugins of a given type.
	static std::list<Plugin*>
	lookup(Type type);

	// TODO: check if this makes sense! (no intermediate plugins)
	bool
	operator==(const Plugin& other) const;

	Type pluginType;
	std::string name;
	std::string description;
	std::string path;
	void *handle;
	enum state state;

protected:
	static SpdLogger
	getStaticLogger()
	{ return loggerGetOrCreate("plugin"); }

private:
	/* Just using a standard std::list<> to hold plugins is problematic, because
	   we want to push Plugins to the list from within each Plugin's constructor
	   that is executed during static initialization. Since the order of static
	   initialization is undefined in C++, it may happen that a Plugin
	   constructor is executed before the list could be initialized. Therefore,
	   we use the Nifty Counter Idiom [1] to initialize the list ourself before
	   the first usage.

		In short:
		- allocate a buffer for the list
		- initialize list before first usage
		- (complicatedly) declaring a buffer is neccessary in order to avoid
		  that the constructor of the static list is executed again

	   [1] https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Nifty_Counter
	*/

	using PluginList = std::list<Plugin *>;
	using PluginListBuffer = typename std::aligned_storage<sizeof (Plugin::PluginList), alignof (Plugin::PluginList)>::type;

	static PluginListBuffer pluginListBuffer;	///< buffer to hold a PluginList
	static PluginList& pluginList;		///< reference to pluginListBuffer
	static int pluginListNiftyCounter;	///< track if pluginList has been initialized
};

} // namespace villas
