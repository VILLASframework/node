/** REST-API-releated functions.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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
#include <libwebsockets.h>

#include <atomic>
#include <thread>
#include <list>

#include <libwebsockets.h>

#include <villas/log.hpp>
#include <villas/common.hpp>
#include <villas/queue_signalled.hpp>
#include <villas/exceptions.hpp>

namespace villas {
namespace node {
namespace api {

const int version = 2;

/* Forward declarations */
class Session;
class Response;
class Request;

class Error : public RuntimeError {

public:

	template<typename... Args>
	Error(int c = HTTP_STATUS_INTERNAL_SERVER_ERROR, const std::string &msg = "Invalid API request", Args&&... args) :
		RuntimeError(msg, std::forward<Args>(args)...),
		code(c),
		json(nullptr)
	{ }

	template<typename... Args>
	Error(int c = HTTP_STATUS_INTERNAL_SERVER_ERROR, const std::string &msg = "Invalid API request", const char *fmt = nullptr, Args&&... args) :
		RuntimeError(msg),
		code(c),
		json(fmt
			? json_pack(fmt, std::forward<Args>(args)...)
			: nullptr
		)
	{ }

	int code;
	json_t *json;
};

class BadRequest : public Error {

public:
	template<typename... Args>
	BadRequest(const std::string &msg = "Bad API request", Args&&... args) :
		Error(HTTP_STATUS_BAD_REQUEST, msg, std::forward<Args>(args)...)
	{ }
};

class InvalidMethod : public BadRequest {

public:
	InvalidMethod(Request *req);
};

} /* namespace api */

/* Forward declarations */
class SuperNode;

class Api {

protected:
	Logger logger;

	enum State state;

	std::thread thread;
	std::atomic<bool> running;	/**< Atomic flag for signalizing thread termination. */

	SuperNode *super_node;

	void run();
	void worker();

public:
	/** Initalize the API.
	 *
	 * Save references to list of paths / nodes for command execution.
	 */
	Api(SuperNode *sn);
	~Api();

	void start();
	void stop();

	SuperNode * getSuperNode()
	{
		return super_node;
	}

	std::list<api::Session *> sessions;		/**< List of currently active connections */
	villas::QueueSignalled<api::Session *> pending;	/**< A queue of api_sessions which have pending requests. */
};

} /* namespace node */
} /* namespace villas */
