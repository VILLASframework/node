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

#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <memory>

#include <string.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <villas/copyright.hpp>

#include <libwebsockets.h>

auto console = spdlog::stdout_color_mt("console");

/** The libwebsockets server context. */
static lws_context *context;

/** The libwebsockets vhost. */
static lws_vhost *vhost;

/* Forward declarations */
lws_callback_function protocol_cb;
class Session;
class Connection;

static std::map<std::string, std::shared_ptr<Session>> sessions;

class InvalidUrlException { };

struct Options {
	bool loopback;
} opts;

class Frame : public std::vector<uint8_t> {
public:
	Frame() {
		// lws_write() requires LWS_PRE bytes in front of the payload
		insert(end(), LWS_PRE, 0);
	}

	uint8_t * data() {
		return std::vector<uint8_t>::data() + LWS_PRE;
	}

	size_type size() {
		return std::vector<uint8_t>::size() - LWS_PRE;
	}
};

class Session {
public:
	typedef std::string Identifier;

	static std::shared_ptr<Session> get(lws *wsi)
	{
		char uri[64];

		/* We use the URI to associate this connection to a session
		 * Example: ws://example.com/node_1
		 *   Will select the session with the name 'node_1'
		 */

		/* Get path of incoming request */
		lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI);
		if (strlen(uri) <= 0)
			throw InvalidUrlException();

		Identifier sid = uri;

		auto it = sessions.find(sid);
		if (it == sessions.end()) {
			auto s = std::make_shared<Session>(sid);

			sessions[sid] = s;

			console->debug("Creating new session: {}", sid);

			return s;
		}
		else {
			console->debug("Found existing session: {}", sid);

			return it->second;
		}
	}

	Session(Identifier sid) :
		identifier(sid)
	{ }

	~Session()
	{ }

	Identifier identifier;

	std::map<lws *, std::shared_ptr<Connection>> connections;
};

class Connection {

protected:
	lws *wsi;

	std::shared_ptr<Frame> currentFrame;

	std::queue<std::shared_ptr<Frame>> outgoingFrames;

	std::shared_ptr<Session> session;

public:
	Connection(lws *w) :
		wsi(w),
		currentFrame(std::make_shared<Frame>()),
		outgoingFrames()
	{
		session = Session::get(wsi);
		session->connections[wsi] = std::shared_ptr<Connection>(this);

		console->info("New connection established to session: {}", session->identifier);
	}

	~Connection() {
		console->info("Connection closed");

		session->connections.erase(wsi);
	}

	void write() {
		while (!outgoingFrames.empty()) {
			std::shared_ptr<Frame> fr = outgoingFrames.front();

			lws_write(wsi, fr->data(), fr->size(), LWS_WRITE_BINARY);

			outgoingFrames.pop();
		}
	}

	void read(void *in, size_t len) {
		currentFrame->insert(currentFrame->end(), (uint8_t *) in, (uint8_t *) in + len);

		if (lws_is_final_fragment(wsi)) {
			console->debug("Received frame, relaying to {} connections", session->connections.size() - (opts.loopback ? 0 : 1));

			for (auto p : session->connections) {
				auto c = p.second;

				/* We skip the current connection in order
				 * to avoid receiving our own data */
				if (opts.loopback == false && c.get() == this)
					continue;

				c->outgoingFrames.push(currentFrame);

				lws_callback_on_writable(c->wsi);
			}

			currentFrame = std::make_shared<Frame>();
		}
	}
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

static void logger(int level, const char *msg) {
	auto log = spdlog::get("lws");

	int len = strlen(msg);
	if (strchr(msg, '\n'))
		len -= 1;

	/* Decrease severity for some errors. */
	if (strstr(msg, "Unable to open") == msg)
		level = LLL_WARN;

	switch (level) {
		case LLL_ERR:   log->error("{}", msg); break;
		case LLL_WARN:	log->warn( "{}", msg); break;
		case LLL_INFO:	log->info( "{}", msg); break;
		default:        log->debug("{}", msg); break;
	}
}

int protocol_cb(lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	Connection *c = reinterpret_cast<Connection *>(user);

	switch (reason) {

		case LWS_CALLBACK_ESTABLISHED: {
			auto s = Session::get(wsi);

			try {
				new (c) Connection(wsi);
			}
			catch (InvalidUrlException e) {
				lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, (unsigned char *) "Invalid URL", strlen("Invalid URL"));
				return -1;
			}
			break;
		}

		case LWS_CALLBACK_CLOSED:
			c->~Connection();
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			c->write();
			break;

		case LWS_CALLBACK_RECEIVE:
			c->read(in, len);
			break;

		default:
			break;
	}

	return 0;
}

void usage()
{
	std::cout << "Usage: villas-relay [OPTIONS]" << std::endl;
	std::cout << "  OPTIONS is one or more of the following options:" << std::endl;
	std::cout << "    -d LVL    set debug level" << std::endl;
	std::cout << "    -p PORT   the port number to listen on" << std::endl;
	std::cout << "    -P PROT   the websocket protocol" << std::endl;
	std::cout << "    -V        show version and exit" << std::endl;
	std::cout << "    -h        show usage and exit" << std::endl;
	std::cout << std::endl;

	villas::print_copyright();
}

int main(int argc, char *argv[])
{
	/* Initialize logging */
	spdlog::stdout_color_mt("lws");
	lws_set_log_level((1 << LLL_COUNT) - 1, logger);

	/* Start server */
	lws_context_creation_info ctx_info = { 0 };

	ctx_info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS | LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	ctx_info.gid = -1;
	ctx_info.uid = -1;
	ctx_info.protocols = protocols;
	ctx_info.extensions = extensions;
	ctx_info.port = 8088;

	char c, *endptr;
	while ((c = getopt (argc, argv, "hVp:P:l")) != -1) {
		switch (c) {
			case 'p':
				ctx_info.port = strtoul(optarg, &endptr, 10);
				goto check;

			case 'P':
				protocols[0].name = optarg;
				break;

			case 'l':
				opts.loopback = true;
				break;

			case 'V':
				villas::print_version();
				exit(EXIT_SUCCESS);

			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;

check:		if (optarg == endptr) {
			console->error("Failed to parse parse option argument '-{} {}'", c, optarg);
			exit(EXIT_FAILURE);
		}
	}

	if (argc - optind < 0) {
		usage();
		exit(EXIT_FAILURE);
	}

	context = lws_create_context(&ctx_info);
	if (context == NULL) {
		console->error("WebSocket: failed to initialize server context");
		exit(EXIT_FAILURE);
	}

	vhost = lws_create_vhost(context, &ctx_info);
	if (vhost == NULL) {
		console->error("WebSocket: failed to initialize virtual host");
		exit(EXIT_FAILURE);
	}

	for (;;)
		lws_service(context, 100);

	return 0;
}
