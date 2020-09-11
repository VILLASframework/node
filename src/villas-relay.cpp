/** Simple WebSocket relay facilitating client-to-client connections.
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

#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <utility>

#include <cstring>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <jansson.h>

#include <villas/node/config.h>
#include <villas/compat.hpp>
#include <villas/memory.h>
#include <villas/tool.hpp>
#include <villas/log.hpp>

#include "villas-relay.hpp"


namespace villas {
namespace node {
namespace tools {

RelaySession::RelaySession(Identifier sid) :
	identifier(sid),
	connects(0)
{
	Logger logger = villas::logging.get("console");

	logger->info("RelaySession created: {}", identifier);

	sessions[sid] = this;

	created = time(nullptr);

	uuid_generate(uuid);
}

RelaySession::~RelaySession()
{
	Logger logger = villas::logging.get("console");

	logger->info("RelaySession destroyed: {}", identifier);

	sessions.erase(identifier);
}

RelaySession * RelaySession::get(lws *wsi)
{
	Logger logger = villas::logging.get("console");

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
		auto *rs = new RelaySession(sid);
		if (!rs)
			throw MemoryAllocationError();

		return rs;
	}
	else {
		logger->info("Found existing session: {}", sid);

		return it->second;
	}
}

json_t * RelaySession::toJson() const
{
	json_t *json_connections = json_array();

	for (auto it : connections) {
		auto conn = it.second;

		json_array_append(json_connections, conn->toJson());
	}

	char uuid_str[UUID_STR_LEN];
	uuid_unparse_lower(uuid, uuid_str);

	return json_pack("{ s: s, s: s, s: o, s: I, s: i }",
		"identifier", identifier.c_str(),
		"uuid", uuid_str,
		"connections", json_connections,
		"created", created,
		"connects", connects
	);
}

std::map<std::string, RelaySession *> RelaySession::sessions;

RelayConnection::RelayConnection(lws *w, bool lo) :
	wsi(w),
	currentFrame(std::make_shared<Frame>()),
	outgoingFrames(),
	bytes_recv(0),
	bytes_sent(0),
	frames_recv(0),
	frames_sent(0),
	loopback(lo)
{
	Logger logger = villas::logging.get("console");

	session = RelaySession::get(wsi);
	session->connections[wsi] = this;
	session->connects++;

	lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi), name, sizeof(name), ip, sizeof(ip));

	created = time(nullptr);

	logger->info("New connection established: session={}, remote={} ({})", session->identifier, name, ip);
}

RelayConnection::~RelayConnection()
{
	Logger logger = villas::logging.get("console");

	logger->info("RelayConnection closed: session={}, remote={} ({})", session->identifier, name, ip);

	session->connections.erase(wsi);

	if (session->connections.empty())
		delete session;
}

json_t * RelayConnection::toJson() const
{
	return json_pack("{ s: s, s: s, s: I, s: I, s: I, s: I, s: I }",
		"name", name,
		"ip", ip,
		"created", created,
		"bytes_recv", bytes_recv,
		"bytes_sent", bytes_sent,
		"frames_recv", frames_recv,
		"frames_sent", frames_sent
	);
}

void RelayConnection::write()
{
	int ret;

	std::shared_ptr<Frame> fr = outgoingFrames.front();

	ret = lws_write(wsi, fr->data(), fr->size(), LWS_WRITE_BINARY);
	if (ret < 0)
		return;

	bytes_sent += fr->size();
	frames_sent++;

	outgoingFrames.pop();

	if (outgoingFrames.size() > 0)
		lws_callback_on_writable(wsi);
}

void RelayConnection::read(void *in, size_t len)
{
	Logger logger = villas::logging.get("console");

	currentFrame->insert(currentFrame->end(), (uint8_t *) in, (uint8_t *) in + len);

	bytes_recv += len;

	if (lws_is_final_fragment(wsi)) {
		frames_recv++;
		logger->debug("Received frame, relaying to {} connections", session->connections.size() - (loopback ? 0 : 1));

		for (auto p : session->connections) {
			auto c = p.second;

			/* We skip the current connection in order
				* to avoid receiving our own data */
			if (loopback == false && c == this)
				continue;

			c->outgoingFrames.push(currentFrame);

			lws_callback_on_writable(c->wsi);
		}

		currentFrame = std::make_shared<Frame>();
	}
}

Relay::Relay(int argc, char *argv[]) :
	Tool(argc, argv, "relay"),
	stop(false),
	context(nullptr),
	vhost(nullptr),
	loopback(false),
	port(8088),
	protocol("live")
{
	int ret;

	ret = memory_init(0);
	if (ret)
		throw RuntimeError("Failed to initialize memory");

	/* Initialize logging */
	spdlog::stdout_color_mt("lws");
	lws_set_log_level((1 << LLL_COUNT) - 1, logger_cb);

	protocols = {
		{
			.name = "http",
			.callback = lws_callback_http_dummy,
			.per_session_data_size = 0,
			.rx_buffer_size = 1024
		},
		{
			.name = "http-api",
			.callback = http_protocol_cb,
			.per_session_data_size = 0,
			.rx_buffer_size = 1024
		},
		{
			.name = "live",
			.callback = protocol_cb,
			.per_session_data_size = sizeof(RelayConnection),
			.rx_buffer_size = 0
		},
		{ nullptr /* terminator */ }
	};
}

void Relay::logger_cb(int level, const char *msg)
{
	auto log = spdlog::get("lws");

	char *nl = (char *) strchr(msg, '\n');
	if (nl)
		*nl = 0;

	/* Decrease severity for some errors. */
	if (strstr(msg, "Unable to open") == msg)
		level = LLL_WARN;

	switch (level) {
		case LLL_ERR:
			log->error("{}", msg);
			break;

		case LLL_WARN:
			log->warn( "{}", msg);
			break;

		case LLL_INFO:
			log->info( "{}", msg);
			break;

		default:
			log->debug("{}", msg);
			break;
	}
}

int Relay::http_protocol_cb(lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;
	size_t json_len;
	json_t *json_sessions, *json_body;

	lws_context *ctx = lws_get_context(wsi);
	void *user_ctx = lws_context_user(ctx);

	Relay           *r = reinterpret_cast<Relay *>(user_ctx);

	unsigned char buf[LWS_PRE + 2048], *start = &buf[LWS_PRE], *end = &buf[sizeof(buf) - LWS_PRE - 1], *p = start;

	switch (reason) {
		case LWS_CALLBACK_HTTP:
			if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
					"application/json",
					LWS_ILLEGAL_HTTP_CONTENT_LEN, /* no content len */
					&p, end))
				return 1;

			if (lws_finalize_write_http_header(wsi, start, &p, end))
				return 1;

			/* Write the body separately */
			lws_callback_on_writable(wsi);

			return 0;

		case LWS_CALLBACK_HTTP_WRITEABLE:

			json_sessions = json_array();
			for (auto it : RelaySession::sessions) {
				auto &session = it.second;

				json_array_append(json_sessions, session->toJson());
			}

			char hname[128];
			gethostname(hname, 128);

			json_body = json_pack("{ s: o, s: s, s: s, s: { s: b, s: i, s: s } }",
				"sessions", json_sessions,
				"version", PROJECT_VERSION_STR,
				"hostname", hname,
				"options",
					"loopback", r->loopback,
					"port", r->port,
					"protocol", r->protocol.c_str()
			);

			json_len = json_dumpb(json_body, (char *) buf + LWS_PRE, sizeof(buf) - LWS_PRE, JSON_INDENT(4));

			ret = lws_write(wsi, buf + LWS_PRE, json_len, LWS_WRITE_HTTP_FINAL);
			if (ret < 0)
				return ret;

			r->logger->info("Handled API request");

			//if (lws_http_transaction_completed(wsi))
				return -1;

		default:
			break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

int Relay::protocol_cb(lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	lws_context *ctx = lws_get_context(wsi);
	void *user_ctx = lws_context_user(ctx);

	Relay           *r = reinterpret_cast<Relay *>(user_ctx);
	RelayConnection *c = reinterpret_cast<RelayConnection *>(user);

	switch (reason) {

		case LWS_CALLBACK_ESTABLISHED:
			try {
				new (c) RelayConnection(wsi, r->loopback);
			}
			catch (const InvalidUrlException &e) {
				lws_close_reason(wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, (unsigned char *) "Invalid URL", strlen("Invalid URL"));

				return -1;
			}
			break;

		case LWS_CALLBACK_CLOSED:
			c->~RelayConnection();
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

void Relay::usage()
{
	std::cout << "Usage: villas-relay [OPTIONS]" << std::endl
		<< "  OPTIONS is one or more of the following options:" << std::endl
		<< "    -d LVL    set debug level" << std::endl
		<< "    -p PORT   the port number to listen on" << std::endl
		<< "    -P PROT   the websocket protocol" << std::endl
		<< "    -l        enable loopback of own data" << std::endl
		<< "    -V        show version and exit" << std::endl
		<< "    -h        show usage and exit" << std::endl << std::endl;

	printCopyright();
}

void Relay::parse()
{
	char c, *endptr;
	while ((c = getopt (argc, argv, "hVp:P:ld:")) != -1) {
		switch (c) {
			case 'd':
				spdlog::set_level(spdlog::level::from_str(optarg));
				break;

			case 'p':
				port = strtoul(optarg, &endptr, 10);
				goto check;

			case 'P':
				protocol = optarg;
				break;

			case 'l':
				loopback = true;
				break;

			case 'V':
				printVersion();
				exit(EXIT_SUCCESS);

			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;

check:		if (optarg == endptr) {
			logger->error("Failed to parse parse option argument '-{} {}'", c, optarg);
			exit(EXIT_FAILURE);
		}
	}

	if (argc - optind < 0) {
		usage();
		exit(EXIT_FAILURE);
	}
}

int Relay::main() {
	/* Start server */
	lws_context_creation_info ctx_info = { 0 };

	protocols[2].name = protocol.c_str();

	ctx_info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS | LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	ctx_info.gid = -1;
	ctx_info.uid = -1;
	ctx_info.protocols = protocols.data();
	ctx_info.extensions = extensions.data();
	ctx_info.port = port;
	ctx_info.mounts = &mount;
	ctx_info.user = (void *) this;

	context = lws_create_context(&ctx_info);
	if (context == nullptr) {
		logger->error("WebSocket: failed to initialize server context");
		exit(EXIT_FAILURE);
	}

	vhost = lws_create_vhost(context, &ctx_info);
	if (vhost == nullptr) {
		logger->error("WebSocket: failed to initialize virtual host");
		exit(EXIT_FAILURE);
	}

	while (!stop)
		lws_service(context, 100);

	return 0;
}

const std::vector<lws_extension> Relay::extensions = {
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
	{ nullptr /* terminator */ }
};

const lws_http_mount Relay::mount = {
	.mount_next =		nullptr, /* linked-list "next" */
	.mountpoint =		"/api/v1", /* mountpoint URL */
	.origin =		nullptr,	/* protocol */
	.def =			nullptr,
	.protocol =		"http-api",
	.cgienv =		nullptr,
	.extra_mimetypes =	nullptr,
	.interpret =		nullptr,
	.cgi_timeout =		0,
	.cache_max_age =	0,
	.auth_mask =		0,
	.cache_reusable =	0,
	.cache_revalidate =	0,
	.cache_intermediaries =	0,
	.origin_protocol =	LWSMPRO_CALLBACK, /* dynamic */
	.mountpoint_len =	7, /* char count */
	.basic_auth_login_file =nullptr,
};

} /* namespace tools */
} /* namespace node */
} /* namespace villas */

int main(int argc, char *argv[])
{
	villas::node::tools::Relay t(argc, argv);

	return t.run();
}


