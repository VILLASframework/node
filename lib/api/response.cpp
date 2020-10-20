/** API Response.
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

#include <villas/api/response.hpp>
#include <villas/api/request.hpp>

using namespace villas::node::api;

Response::Response(Session *s, int c, const std::string &ct, const Buffer &b) :
	session(s),
	logger(logging.get("api:response")),
	buffer(b),
	code(c),
	contentType(ct),
	headers{
		{ "Server:", HTTP_USER_AGENT },
		{ "Access-Control-Allow-Origin:", "*" },
		{ "Access-Control-Allow-Methods:", "GET, POST, OPTIONS" },
		{ "Access-Control-Allow-Headers:", "Content-Type" },
		{ "Access-Control-Max-Age:", "86400" }
	}
{ }

int
Response::writeBody(struct lws *wsi)
{
	int ret;

	ret = lws_write(wsi, (unsigned char *) buffer.data(), buffer.size(), LWS_WRITE_HTTP_FINAL);
	if (ret < 0)
		return -1;

	return 1;
}

int
Response::writeHeaders(struct lws *wsi)
{
	int ret;
	uint8_t headerBuffer[2048], *p = headerBuffer, *end = &headerBuffer[sizeof(headerBuffer) - 1];

	// We need to encode the buffer here for getting the real content length of the response
	encodeBody();

	ret = lws_add_http_common_headers(wsi, code, contentType.c_str(),
		buffer.size(),
		&p, end);
	if (ret)
		return 1;

	for (auto &hdr : headers) {
		ret = lws_add_http_header_by_name (wsi,
			reinterpret_cast<const unsigned char *>(hdr.first.c_str()),
			reinterpret_cast<const unsigned char *>(hdr.second.c_str()),
			hdr.second.size(), &p, end);
		if (ret)
			return -1;
	}

	ret = lws_finalize_write_http_header(wsi, headerBuffer, &p, end);
	if (ret)
		return 1;

	/* Do we have a body to send? */
	if (buffer.size() > 0)
		lws_callback_on_writable(wsi);

	return 0;
}

JsonResponse::~JsonResponse()
{
	if (response)
		json_decref(response);
}

void
JsonResponse::encodeBody()
{
	buffer.encode(response, JSON_INDENT(4));
}
