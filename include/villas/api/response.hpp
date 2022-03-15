/** API response.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <map>

#include <jansson.h>

#include <villas/log.hpp>
#include <villas/buffer.hpp>
#include <villas/exceptions.hpp>
#include <villas/plugin.hpp>
#include <villas/api.hpp>

namespace villas {
namespace node {
namespace api {

/* Forward declarations */
class Session;
class Request;

class Response {

public:
	friend Session;

	Response(Session *s, int c = HTTP_STATUS_OK, const std::string &ct = "text/html; charset=UTF-8", const Buffer &b = Buffer());

	virtual
	~Response()
	{ }

	virtual void
	encodeBody()
	{ }

	int
	writeBody(struct lws *wsi);

	int
	writeHeaders(struct lws *wsi);

	void
	setHeader(const std::string &key, const std::string &value)
	{
		headers[key] = value;
	}

protected:
	Session *session;
	Logger logger;
	Buffer buffer;

	int code;
	std::string contentType;
	std::map<std::string, std::string> headers;
};

class JsonResponse : public Response {

protected:
	json_t *response;

public:
	JsonResponse(Session *s, int c, json_t *r) :
		Response(s, c, "application/json"),
		response(r)
	{ }

	virtual ~JsonResponse();

	virtual void
	encodeBody();
};

class ErrorResponse : public JsonResponse {

public:
	ErrorResponse(Session *s, const RuntimeError &e) :
		JsonResponse(s, HTTP_STATUS_INTERNAL_SERVER_ERROR, json_pack("{ s: s }", "error", e.what()))
	{ }

	ErrorResponse(Session *s, const Error &e) :
		JsonResponse(s, e.code, json_pack("{ s: s }", "error", e.what()))
	{
		if (e.json)
			json_object_update(response, e.json);
	}
};

} /* namespace api */
} /* namespace node */
} /* namespace villas */
