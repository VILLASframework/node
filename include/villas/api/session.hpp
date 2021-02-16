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
#include <villas/buffer.hpp>
#include <villas/api.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class SuperNode;
class Api;
class Web;

namespace api {

/* Forward declarations */
class Request;
class Response;
class StatusRequest;
class RequestFactory;

/** A connection via HTTP REST or WebSockets to issue API requests. */
class Session {

public:
	friend Request; /**< Requires access to wsi for getting URL args and headers */
	friend StatusRequest; /**< Requires access to wsi for context status */
	friend RequestFactory;

	enum State {
		ESTABLISHED,
		SHUTDOWN
	};

	enum Version {
		UNKNOWN_VERSION	= 0,
		VERSION_1	= 1,
		VERSION_2	= 2
	};

	enum Method {
		UNKNOWN,
		GET,
		POST,
		DELETE,
		OPTIONS,
		PUT,
		PATCH
	};

protected:
	enum State state;
	enum Version version;

	lws *wsi;

	Web *web;
	Api *api;

	Logger logger;

	std::unique_ptr<Request>  request;
	std::unique_ptr<Response> response;

	bool headersSent;

public:
	Session(struct lws *w);
	~Session();

	std::string getName() const;

	SuperNode * getSuperNode() const
	{
		return api->getSuperNode();
	}

	void open(void *in, size_t len);
	int writeable();
	void body(void *in, size_t len);
	void bodyComplete();
	void execute();
	void shutdown();

	static int
	protocolCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

	Method getRequestMethod() const;

	static std::string
	methodToString(Method meth);

};

} /* namespace api */
} /* namespace node */
} /* namespace villas */
