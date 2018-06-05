/** Simple WebSocket relay facilitating client-to-client connections.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <vector>
#include <map>
#include <deque>
#include <string>
#include <memory>

#include <string.h>

#include <libwebsockets.h>

#include <villas/utils.h>

/** The libwebsockets server context. */
static lws_context *context;

/** The libwebsockets vhost. */
static lws_vhost *vhost;

/* Forward declarations */
lws_callback_function protocol_cb;
struct Session;
struct Connection;

static std::map<std::string, std::shared_ptr<Session>> sessions;

class Frame : public std::vector<uint8_t> { };

class Session {
public:
	typedef std::string Identifier;


	static std::shared_ptr<Session> get(Identifier sid)
	{
		auto it = sessions.find(sid);
		if (it == sessions.end()) {
			auto s = std::make_shared<Session>(sid);

			sessions[sid] = s;

			return s;
		}
		else
			return it->second;
	}

	Session(Identifier sid) :
		identifier(sid)
	{ }

	~Session()
	{ }

	Identifier identifier;

	std::vector<std::shared_ptr<Connection>> connections;
};

class Connection {
public:
	lws *wsi;

	std::shared_ptr<Frame> currentFrame;

	std::deque<std::shared_ptr<Frame>> outgoingFrames;	

	std::shared_ptr<Session> session;

	Connection() :
		currentFrame(nullptr)
	{ }
};

/** List of libwebsockets protocols. */
lws_protocols protocols[] = {
	{
		.name = "live",
		.callback = protocol_cb,
		.per_session_data_size = sizeof(Connection),
		.rx_buffer_size = 0
	},
	{ NULL /* terminator */ }
};

/** List of libwebsockets extensions. */
static const lws_extension extensions[] = {
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate"
	},
	{
		"deflate-frame",
		lws_extension_callback_pm_deflate,
		"deflate_frame"
	},
	{ NULL /* terminator */ }
};

extern log *global_log;

static void logger(int level, const char *msg) {
	int len = strlen(msg);
	if (strchr(msg, '\n'))
		len -= 1;

	/* Decrease severity for some errors. */
	if (strstr(msg, "Unable to open") == msg)
		level = LLL_WARN;

	switch (level) {
		case LLL_ERR:   log_print(global_log, CLR_RED("Web  "), "%.*s", len, msg); break;
		case LLL_WARN:	log_print(global_log, CLR_YEL("Web  "), "%.*s", len, msg); break;
		case LLL_INFO:	log_print(global_log, CLR_WHT("Web  "), "%.*s", len, msg); break;
		default:        log_print(global_log,         "Web  ",  "%.*s", len, msg); break;
	}
}

int protocol_cb(lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	Connection *c = reinterpret_cast<Connection *>(user);

	switch (reason) {

		case LWS_CALLBACK_ESTABLISHED: {
			char uri[64];

			new (c) Connection();

			c->wsi = wsi;

			/* We use the URI to associate this connection to a session
			 * Example: ws://example.com/node_1
			 *   Will select the session with the name 'node_1'
			 */

			/* Get path of incoming request */
			lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI);
			if (strlen(uri) <= 0) {
				lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, (unsigned char *) "Invalid URL", strlen("Invalid URL"));
				return -1;
			}

			Session::Identifier sid = &uri[1];

			auto s = Session::get(sid);

			c->session = s;
			s->connections.push_back(std::shared_ptr<Connection>(c));

			break;
		}

		case LWS_CALLBACK_CLOSED:

			c->~Connection();
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			while (c->outgoingFrames.size() > 0) {
				std::shared_ptr<Frame> fr = c->outgoingFrames.front();

				lws_write(wsi, fr->data(), fr->size(), LWS_WRITE_BINARY);

				c->outgoingFrames.pop_front();
			}
			break;

		case LWS_CALLBACK_RECEIVE:
			if (!c->currentFrame.get() || lws_is_first_fragment(wsi))
				c->currentFrame = std::make_shared<Frame>();

			c->currentFrame->insert(c->currentFrame->end(), (uint8_t *) in, (uint8_t *) in + len);

			if (lws_is_final_fragment(wsi)) {
				for (auto cc : c->session->connections) {
					if (cc.get() == c)
						continue;

					cc->outgoingFrames.push_back(c->currentFrame);

					lws_callback_on_writable(cc->wsi);
				}
			}
			
			break;

		default:
			break;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	lws_set_log_level((1 << LLL_COUNT) - 1, logger);

	/* Start server */
	lws_context_creation_info ctx_info = { 0 };

	ctx_info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS | LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	ctx_info.gid = -1;
	ctx_info.uid = -1;
	ctx_info.protocols = protocols;
	ctx_info.extensions = extensions;
	ctx_info.port = 8088;

	context = lws_create_context(&ctx_info);
	if (context == NULL)
		error("WebSocket: failed to initialize server context");

	vhost = lws_create_vhost(context, &ctx_info);
	if (vhost == NULL)
		error("WebSocket: failed to initialize virtual host");

	for (;;)
		lws_service(context, 100);

	return 0;
}
