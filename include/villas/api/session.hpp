/* API session.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <jansson.h>

#include <villas/queue.h>
#include <villas/buffer.hpp>
#include <villas/api.hpp>

namespace villas {
namespace node {

// Forward declarations
class SuperNode;
class Api;
class Web;

namespace api {

// Forward declarations
class Request;
class Response;
class StatusRequest;
class RequestFactory;

// A connection via HTTP REST or WebSockets to issue API requests.
class Session {

public:
	friend Request; // Requires access to wsi for getting URL args and headers
	friend StatusRequest; // Requires access to wsi for context status
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

	static
	int protocolCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

	Method getRequestMethod() const;

	static
	std::string methodToString(Method meth);
};

} // namespace api
} // namespace node
} // namespace villas
