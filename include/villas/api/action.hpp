/** REST-API-releated functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

	static Logger logger;

public:
	Action(Session *s) :
		session(s)
	{ }

	virtual int execute(json_t *args, json_t **resp) = 0;
};

class ActionFactory : public plugin::Plugin {

public:
	using plugin::Plugin::Plugin;

	virtual Action * make(Session *s) = 0;
};

template<typename T>
class ActionPlugin : public ActionFactory {

public:
	using ActionFactory::ActionFactory;

	virtual Action * make(Session *s) {
		return new T(s);
	};
};

} // api
} // node
} // villas
