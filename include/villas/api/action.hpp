/** REST-API-releated functions.
 *
 * @file
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

#pragma once

#include <jansson.h>

#include <villas/log.hpp>
#include <villas/plugin.hpp>

namespace villas {
namespace node {
namespace api {

/* Forward declarations */
class Session;

/** API action descriptor  */
class Action {

protected:
	Session *session;

	Logger logger;

public:
	Action(Session *s) :
		session(s)
	{
		logger = logging.get("api:action");
	}

	virtual int execute(json_t *args, json_t **resp) = 0;
};

class ActionFactory : public plugin::Plugin {

public:
	using plugin::Plugin::Plugin;

	virtual Action * make(Session *s) = 0;
};

template<typename T, const char *name, const char *desc>
class ActionPlugin : public ActionFactory {

public:
	using ActionFactory::ActionFactory;

	virtual Action * make(Session *s) {
		return new T(s);
	}

	// Get plugin name
	virtual std::string
	getName() const
	{ return name; }

	// Get plugin description
	virtual std::string
	getDescription() const
	{ return desc; }
};

} // api
} // node
} // villas
