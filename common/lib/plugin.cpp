/** Loadable / plugin support.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <iostream>
#include <string>
#include <new>
#include <type_traits>
#include <dlfcn.h>

#include <villas/plugin.hpp>

using namespace villas::plugin;

List<> * Registry::plugins;

Plugin::Plugin()
{
	Registry::add(this);
}

Plugin::~Plugin()
{
	Registry::remove(this);
}

#if 0
int
Plugin::parse(json_t *cfg)
{
	const char *path;

	path = json_string_value(cfg);
	if (!path)
		return -1;

	this->path = std::string(path);
	this->state = State::PARSED;

	return 0;
}

int
Plugin::load()
{
	assert(this->state == State::PARSED);
	assert(not this->path.empty());

	this->handle = dlopen(this->path.c_str(), RTLD_NOW);

	if (this->handle == nullptr)
		return -1;

	this->state = State::LOADED;

	return 0;
}

int
Plugin::unload()
{
	int ret;

	assert(this->state == State::LOADED);

	ret = dlclose(this->handle);
	if (ret != 0)
		return -1;

	this->state = State::UNLOADED;

	return 0;
}
#endif

void
Plugin::dump()
{
	Logger logger = Registry::getLogger();
	logger->info("Name: '{}' Description: '{}'", getName(), getDescription());
}
