/** API response.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
