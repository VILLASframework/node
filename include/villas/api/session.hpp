/** API session.
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

#include <villas/queue.h>
#include <villas/json_buffer.hpp>
#include <villas/api.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class SuperNode;
class Api;

namespace api {

/** A connection via HTTP REST or WebSockets to issue API actions. */
class Session {

public:
	enum State {
		ESTABLISHED,
		SHUTDOWN
	};

	enum Version {
		UNKOWN		= 0,
		VERSION_1	= 1
	};

protected:
	enum State state;
	enum Version version;

	Logger logger;

	int runs;

	struct {
		JsonBuffer buffer;
		Queue<json_t *> queue;
	} request, response;

	Api *api;

public:
	Session(Api *a);

	virtual ~Session();

	int runAction(json_t *req, json_t **resp);
	virtual void runPendingActions();

	virtual std::string getName();

	int getRuns()
	{
		return runs;
	}

	SuperNode * getSuperNode()
	{
		return api->getSuperNode();
	}

	virtual void shutdown()
	{ }
};

} /* namespace api */
} /* namespace node */
} /* namespace villas */
