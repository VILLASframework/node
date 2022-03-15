/** API session.
 *
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

#include <sstream>

#include <libwebsockets.h>

#include <villas/web.hpp>
#include <villas/node/memory.hpp>

#include <villas/api/session.hpp>
#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::api;

Session::Session(lws *w) :
	version(Version::VERSION_2),
	wsi(w),
	logger(logging.get("api:session"))
{
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

	logger->debug("Destroyed API session: {}", getName());
}

void Session::execute()
{
	logger->debug("Running API request: {}", request->toString());

	try {
		response = std::unique_ptr<Response>(request->execute());

		logger->debug("Completed API request: {}", request->toString());
	} catch (const Error &e) {
		response = std::make_unique<ErrorResponse>(this, e);

		logger->warn("API request failed: {}, code={}: {}", request->toString(), e.code, e.what());
	} catch (const RuntimeError &e) {
		response = std::make_unique<ErrorResponse>(this, e);

		logger->warn("API request failed: {}: {}", request->toString(), e.what());
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

	auto uri = reinterpret_cast<char *>(in);

	try {
		unsigned int len;
		auto method = getRequestMethod();
		if (method == Method::UNKNOWN)
			throw RuntimeError("Invalid request method");

		ret = lws_hdr_copy(wsi, buf, sizeof(buf), WSI_TOKEN_HTTP_CONTENT_LENGTH);
		if (ret < 0)
			throw RuntimeError("Failed to get content length");

		try {
			len = std::stoull(buf);
		} catch (const std::invalid_argument &) {
			len = 0;
		}

		request = std::unique_ptr<Request>(RequestFactory::create(this, uri, method, len));

		/* This is an OPTIONS request.
		 *
		 *  We immediatly send headers and close the connection
		 *  without waiting for a POST body */
		if (method == Method::OPTIONS)
			lws_callback_on_writable(wsi);
		/* This request has no body.
		 * We can reply immediatly */
		else if (len == 0)
			api->pending.push(this);
		else {
			/* This request has a HTTP body. We wait for more data to arrive */
		}
	} catch (const Error &e) {
		response = std::make_unique<ErrorResponse>(this, e);
		lws_callback_on_writable(wsi);
	} catch (const RuntimeError &e) {
		response = std::make_unique<ErrorResponse>(this, e);
		lws_callback_on_writable(wsi);
	}
}

void Session::body(void *in, size_t len)
{
	request->buffer.append((const char *) in, len);
}

void Session::bodyComplete()
{
	try {
		request->decode();

		api->pending.push(this);
	} catch (const Error &e) {
		response = std::make_unique<ErrorResponse>(this, e);

		logger->warn("Failed to decode API request: {}", e.what());
	}
}

int Session::writeable()
{
	if (!response)
		return 0;

	if (!headersSent) {
		response->writeHeaders(wsi);

		/* Now wait, until we can send the body */
		headersSent = true;

		return 0;
	}
	else {
		if (response)
			return response->writeBody(wsi);
		else
			return 0;
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

Session::Method Session::getRequestMethod() const
{
	if      (lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI))
		return Method::GET;
	else if (lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI))
		return Method::POST;
#if defined(LWS_WITH_HTTP_UNCOMMON_HEADERS) || defined(LWS_HTTP_HEADERS_ALL)
	else if (lws_hdr_total_length(wsi, WSI_TOKEN_PUT_URI))
		return Method::PUT;
	else if (lws_hdr_total_length(wsi, WSI_TOKEN_PATCH_URI))
		return Method::PATCH;
	else if (lws_hdr_total_length(wsi, WSI_TOKEN_OPTIONS_URI))
		return Method::OPTIONS;
#endif
	else
		return Method::UNKNOWN;
}

std::string Session::methodToString(Method method)
{
	switch (method) {
		case Method::POST:
			return "POST";

		case Method::GET:
			return "GET";

		case Method::DELETE:
			return "DELETE";

		case Method::PUT:
			return "PUT";

		case Method::PATCH:
			return "GPATCHET";

		case Method::OPTIONS:
			return "OPTIONS";

		default:
			return "UNKNOWN";
	}
}
