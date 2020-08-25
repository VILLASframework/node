/** API session.
 *
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

#include <sstream>

#include <libwebsockets.h>

#include <villas/web.hpp>
#include <villas/plugin.h>
#include <villas/memory.h>

#include <villas/api/session.hpp>
#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::api;

Session::Session(lws *w) :
	wsi(w)
{
	logger = logging.get("api:session");

	lws_context *ctx = lws_get_context(wsi);
	void *user_ctx = lws_context_user(ctx);

	web = static_cast<Web *>(user_ctx);
	api = web->getApi();

	if (!api)
		throw RuntimeError("API is disabled");

	api->sessions.push_back(this);

	logger->debug("Initiated API session: {}", getName());

	state = Session::State::ESTABLISHED;
}

Session::~Session()
{
	api->sessions.remove(this);

	if (request)
		delete request;

	if (response)
		delete response;

	logger->debug("Destroyed API session: {}", getName());
}

void Session::execute()
{
	Request *r = request.exchange(nullptr);

	logger->debug("Running API request: uri={}, method={}", r->uri, r->method);

	try {
		response = req->execute();

		logger->debug("Completed API request: request={}", r->uri, r->method);
	} catch (const Error &e) {
		response = new ErrorResponse(this, e);

		logger->warn("API request failed: uri={}, method={}, code={}: {}", r->uri, r->method, e.code, e.what());
	} catch (const RuntimeError &e) {
		response = new ErrorResponse(this, e);

		logger->warn("API request failed: uri={}, method={}: {}", r->uri, r->method, e.what());
	}

	logger->debug("Ran pending API requests. Triggering on_writeable callback: wsi={}", (void *) wsi);

	web->callbackOnWritable(wsi);
}

std::string Session::getName() const
{
	std::stringstream ss;

	ss << "version=" << version;

	if (wsi) {
		char name[128];
		char ip[128];

		lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi), name, sizeof(name), ip, sizeof(ip));

		ss << ", remote.name=" << name << ", remote.ip=" << ip;
	}

	return ss.str();
}

void Session::shutdown()
{
	state = State::SHUTDOWN;

	web->callbackOnWritable(wsi);
}

void Session::open(void *in, size_t len)
{
	int ret;
	char buf[32];
	unsigned long contentLength;

	int meth;
	std::string uri = reinterpret_cast<char *>(in);

	try {
		meth = getRequestMethod(wsi);
		if (meth == Request::Method::UNKNOWN)
			throw RuntimeError("Invalid request method");

		request = RequestFactory::make(this, uri, meth);

		ret = lws_hdr_copy(wsi, buf, sizeof(buf), WSI_TOKEN_HTTP_CONTENT_LENGTH);
		if (ret < 0)
			throw RuntimeError("Failed to get content length");

		try {
			contentLength = std::stoull(buf);
		} catch (const std::invalid_argument &) {
			contentLength = 0;
		}
	} catch (const Error &e) {
		response = new ErrorResponse(this, e);
		lws_callback_on_writable(wsi);
	} catch (const RuntimeError &e) {
		response = new ErrorResponse(this, e);
		lws_callback_on_writable(wsi);
	}

	/* This is an OPTIONS request.
	 *
	 *  We immediatly send headers and close the connection
	 *  without waiting for a POST body */
	if (meth == Request::Method::OPTIONS)
		lws_callback_on_writable(wsi);
	/* This request has no body.
	 * We can reply immediatly */
	else if (contentLength == 0)
		api->pending.push(this);
	else {
		/* This request has a HTTP body. We wait for more data to arrive */
	}
}

void Session::body(void *in, size_t len)
{
	requestBuffer.append((const char *) in, len);
}

void Session::bodyComplete()
{
	try {
		auto *j = requestBuffer.decode();
		if (!j)
			throw BadRequest("Failed to decode request payload");

		requestBuffer.clear();

		(*request).setBody(j);

		api->pending.push(this);
	} catch (const Error &e) {
		response = new ErrorResponse(this, e);

		logger->warn("Failed to decode API request: {}", e.what());
	}
}

int Session::writeable()
{
	int ret;

	if (!headersSent) {
		int code = HTTP_STATUS_OK;
		const char *contentType = "application/json";

		uint8_t headers[2048], *p = headers, *end = &headers[sizeof(headers) - 1];

		responseBuffer.clear();
		if (response) {
			Response *resp = response;

			json_t *json_response = resp->toJson();

			responseBuffer.encode(json_response, JSON_INDENT(4));

			code = resp->getCode();
		}

		if (lws_add_http_common_headers(wsi, code, contentType,
				responseBuffer.size(), /* no content len */
				&p, end))
			return 1;

		std::map<const char *, const char *> constantHeaders = {
			{ "Server:", USER_AGENT },
			{ "Access-Control-Allow-Origin:", "*" },
			{ "Access-Control-Allow-Methods:", "GET, POST, OPTIONS" },
			{ "Access-Control-Allow-Headers:", "Content-Type" },
			{ "Access-Control-Max-Age:", "86400" }
		};

		for (auto &hdr : constantHeaders) {
			if (lws_add_http_header_by_name (wsi,
					reinterpret_cast<const unsigned char *>(hdr.first),
					reinterpret_cast<const unsigned char *>(hdr.second),
					strlen(hdr.second), &p, end))
				return -1;
		}

		if (lws_finalize_write_http_header(wsi, headers, &p, end))
			return 1;

		/* No wait, until we can send the body */
		headersSent = true;

		/* Do we have a body to send */
		if (responseBuffer.size() > 0)
			lws_callback_on_writable(wsi);

		return 0;
	}
	else {
		ret = lws_write(wsi, (unsigned char *) responseBuffer.data(), responseBuffer.size(), LWS_WRITE_HTTP_FINAL);
		if (ret < 0)
			return -1;

		return 1;
	}
}

int Session::protocolCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;
	Session *s = reinterpret_cast<Session *>(user);

	switch (reason) {
		case LWS_CALLBACK_HTTP_BIND_PROTOCOL:
			try {
				new (s) Session(wsi);
			} catch (const RuntimeError &e) {
				return -1;
			}

			break;

		case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
			if (s == nullptr)
				return -1;

			s->~Session();

			break;

		case LWS_CALLBACK_HTTP:
			s->open(in, len);

			break;

		case LWS_CALLBACK_HTTP_BODY:
			s->body(in, len);

			break;

		case LWS_CALLBACK_HTTP_BODY_COMPLETION:
			s->bodyComplete();

			break;

		case LWS_CALLBACK_HTTP_WRITEABLE:
			ret = s->writeable();

			/*
			 * HTTP/1.0 no keepalive: close network connection
			 * HTTP/1.1 or HTTP1.0 + KA: wait / process next transaction
			 * HTTP/2: stream ended, parent connection remains up
			 */
			if (ret) {
				if (lws_http_transaction_completed(wsi))
					return -1;
			}
			else
				lws_callback_on_writable(wsi);

			break;

		default:
			break;
	}

	return 0;
}

int Session::getRequestMethod(struct lws *wsi)
{
	if      (lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI))
		return Request::Method::GET;
	else if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI))
		return Request::Method::POST;
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_HTTP_HEADERS_ALL)
	else if (lws_hdr_total_length(wsi, WSI_TOKEN_PUT_URI))
		return Request::Method::PUT;
	else if (lws_hdr_total_length(wsi, WSI_TOKEN_PATCH_URI))
		return Request::Method::PATCH;
	else if (lws_hdr_total_length(wsi, WSI_TOKEN_OPTIONS_URI))
		return Request::Method::OPTIONS;
#endif
	else
		return Request::Method::UNKNOWN;
}

std::string Session::methodToString(int method)
{
	switch (method) {
		case Request::Method::POST:
			return "POST";

		case Request::Method::GET:
			return "GET";

		case Request::Method::DELETE:
			return "DELETE";

		case Request::Method::PUT:
			return "PUT";

		case Request::Method::PATCH:
			return "GPATCHET";

		case Request::Method::OPTIONS:
			return "OPTIONS";

		default:
			return "UNKNOWN";
	}
}
