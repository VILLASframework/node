/** Loadable / plugin support.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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

#include <villas/log.hpp>
#include <villas/common.h>

namespace villas {
namespace plugin {

/* Forward declarations */
class Plugin;

template<typename T = Plugin>
using List = std::list<T *>;

class Registry {

protected:
	static List<> *plugins;

public:

	static Logger
	getLogger()
	{
		return logging.get("plugin:registry");
	}

	static void add(Plugin *p)
	{
		if (plugins == nullptr)
			plugins = new List<>;

		plugins->push_back(p);
	}

	static void remove(Plugin *p)
	{
		plugins->remove(p);
	}

	template<typename T = Plugin>
	static T *
	lookup(const std::string &name)
	{
		for (Plugin *p : *plugins) {
			T *t = dynamic_cast<T *>(p);
			if (!t || t->name != name)
				continue;

			return t;
		}

		return nullptr;
	}

	template<typename T = Plugin>
	static List<T>
	lookup()
	{
		List<T> list;

		for (Plugin *p : *plugins) {
			T *t = dynamic_cast<T *>(p);
			if (t)
				list.push_back(t);
		}

		return list;
	}
};

class Loader {

public:
	int load(const std::string &path);
	int unload();

	virtual int parse(json_t *cfg);
};

class Plugin {

	friend plugin::Registry;

public:
	Plugin(const std::string& name, const std::string &desc);
	virtual ~Plugin();

	// copying a plugin doesn't make sense, so explicitly deny it
	Plugin(Plugin const&) = delete;
	void operator=(Plugin const&) = delete;

	virtual void dump();

	std::string getName();
	std::string getDescription();

protected:
	std::string name;
	std::string description;
	std::string path;

	Logger
	getLogger()
	{
		return logging.get("plugin:" + name);
	}
};

} // namespace plugin
} // namespace villas
