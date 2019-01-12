/** WebSockets Api session.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/api/sessions/websocket.hpp>

using namespace villas::node;
using namespace villas::node::api::sessions;

WebSocket::WebSocket(Api *a, lws *w) :
	Wsi(a, w)
{
	/* Parse request URI */
	int version;
	char uri[64];
	lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI);

	sscanf(uri, "/v%d", (int *) &version);

	if (version != api::version)
		throw RuntimeError("Unsupported API version: {}", version);
}

int WebSocket::read(void *in, size_t len)
{
	json_t *req;

	if (lws_is_first_fragment(wsi))
		request.buffer.clear();

	request.buffer.append((const char *) in, len);

	if (lws_is_final_fragment(wsi)) {
		req = request.buffer.decode();
		if (!req)
			return 0;

		request.queue.push(req);

		return 1;
	}

	return 0;
}

int WebSocket::write()
{
	json_t *resp;

	if (state == State::SHUTDOWN)
		return -1;

	resp = response.queue.pop();

	char pad[LWS_PRE];

	response.buffer.clear();
	response.buffer.append(pad, sizeof(pad));
	response.buffer.encode(resp);

	json_decref(resp);

	lws_write(wsi, (unsigned char *) response.buffer.data() + LWS_PRE, response.buffer.size() - LWS_PRE, LWS_WRITE_TEXT);

	return 0;
}

std::string WebSocket::getName()
{
	std::stringstream ss;

	ss << Wsi::getName() << ", mode=ws";

	return ss.str();
}

int api_ws_protocol_cb(lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;

	lws_context *ctx = lws_get_context(wsi);
	void *user_ctx = lws_context_user(ctx);

	Web *w = static_cast<Web*>(user_ctx);
	Api *a = w->getApi();

	WebSocket *s = static_cast<WebSocket *>(user);

	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			if (a == nullptr) {
				std::string err = "API disabled";
				lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, (unsigned char *) err.data(), err.length());
				return -1;
			}

			new (s) WebSocket(a, wsi);

			a->sessions.push_back(s);

			break;

		case LWS_CALLBACK_CLOSED:

			s->~WebSocket();

			a->sessions.remove(s);

			break;

		case LWS_CALLBACK_RECEIVE:
			ret = s->read(in, len);
			if (ret)
				a->pending.push(s);

			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			ret = s->write();
			if (ret)
				return ret;

			break;

		default:
			break;
	}

	return 0;
}
