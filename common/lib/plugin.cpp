/* Loadable / plugin support.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 */

#include <iostream>
#include <string>
#include <new>
#include <type_traits>
#include <dlfcn.h>

#include <villas/plugin.hpp>

using namespace villas::plugin;

Registry * villas::plugin::registry = nullptr;

Plugin::Plugin()
{
	if (registry == nullptr)
		registry = new Registry();

	registry->add(this);
}

Plugin::~Plugin()
{
	registry->remove(this);
}

void
Plugin::dump()
{
	getLogger()->info("Name: '{}' Description: '{}'", getName(), getDescription());
}
