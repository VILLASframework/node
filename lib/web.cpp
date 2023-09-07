/* LWS-releated functions.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>

#include <villas/api.hpp>
#include <villas/api/session.hpp>
#include <villas/config.hpp>
#include <villas/node/config.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/nodes/websocket.hpp>
#include <villas/utils.hpp>
#include <villas/web.hpp>

#ifdef WITH_NODE_WEBRTC
#include <villas/nodes/webrtc/signaling_client.hpp>
#endif

using namespace villas;
using namespace villas::node;

// Forward declarations
lws_callback_function villas::node::websocket_protocol_cb;

// List of libwebsockets protocols.
lws_protocols protocols[] = {
    {.name = "http",
     .callback = lws_callback_http_dummy,
     .per_session_data_size = 0,
     .rx_buffer_size = 1024},
#ifdef WITH_API
    {.name = "http-api",
     .callback = api::Session::protocolCallback,
     .per_session_data_size = sizeof(api::Session),
     .rx_buffer_size = 1024},
#endif // WITH_API
#ifdef WITH_NODE_WEBSOCKET
    {.name = "live",
     .callback = websocket_protocol_cb,
     .per_session_data_size = sizeof(websocket_connection),
     .rx_buffer_size = 0},
#endif // WITH_NODE_WEBSOCKET
#ifdef WITH_NODE_WEBRTC
    {.name = "webrtc-signaling",
     .callback = webrtc::SignalingClient::protocolCallbackStatic,
     .per_session_data_size = sizeof(webrtc::SignalingClient),
     .rx_buffer_size = 0},
#endif
    {
        .name = nullptr,
    }};

// List of libwebsockets mounts.
static lws_http_mount mounts[] = {
#ifdef WITH_API
    {
        .mount_next = nullptr,   // linked-list "next"
        .mountpoint = "/api/v2", // mountpoint URL
        .origin = "http-api",    // protocol
        .def = nullptr,
        .protocol = "http-api",
        .cgienv = nullptr,
        .extra_mimetypes = nullptr,
        .interpret = nullptr,
        .cgi_timeout = 0,
        .cache_max_age = 0,
        .auth_mask = 0,
        .cache_reusable = 0,
        .cache_revalidate = 0,
        .cache_intermediaries = 0,
        .origin_protocol = LWSMPRO_CALLBACK, // dynamic
        .mountpoint_len = 7                  // char count
    }
#endif // WITH_API
};

// List of libwebsockets extensions.
static const lws_extension extensions[] = {
#ifdef LWS_DEFLATE_FOUND
    {.name = "permessage-deflate",
     .callback = lws_extension_callback_pm_deflate,
     .client_offer = "permessage-deflate"},
    {.name = "deflate-frame",
     .callback = lws_extension_callback_pm_deflate,
     .client_offer = "deflate_frame"},
#endif // LWS_DEFLATE_FOUND
    {nullptr /* terminator */}};

void Web::lwsLogger(int lws_lvl, const char *msg) {
  char *nl;

  nl = (char *)strchr(msg, '\n');
  if (nl)
    *nl = 0;

  // Decrease severity for some errors
  if (strstr(msg, "Unable to open") == msg)
    lws_lvl = LLL_WARN;

  Logger logger = logging.get("lws");

  switch (lws_lvl) {
  case LLL_ERR:
    logger->error("{}", msg);
    break;

  case LLL_WARN:
    logger->warn("{}", msg);
    break;

  case LLL_NOTICE:
  case LLL_INFO:
    logger->info("{}", msg);
    break;

  default: // Everything else is debug
    logger->debug("{}", msg);
  }
}

int Web::lwsLogLevel(Log::Level lvl) {
  int lwsLvl = 0;

  switch (lvl) {
  case Log::Level::trace:
    lwsLvl |= LLL_THREAD | LLL_USER | LLL_LATENCY | LLL_CLIENT | LLL_EXT |
              LLL_HEADER | LLL_PARSER;
  case Log::Level::debug:
    lwsLvl |= LLL_DEBUG | LLL_NOTICE | LLL_INFO;
  case Log::Level::info:
  case Log::Level::warn:
    lwsLvl |= LLL_WARN;
  case Log::Level::err:
    lwsLvl |= LLL_ERR;
  case Log::Level::critical:
  case Log::Level::off:
  default: {
  }
  }

  return lwsLvl;
}

void Web::worker() {
  logger->info("Started worker");

  while (running) {
    lws_service(context, 0);

    while (!writables.empty()) {
      auto *wsi = writables.pop();

      lws_callback_on_writable(wsi);
    }
  }

  logger->info("Stopped worker");
}

Web::Web(Api *a)
    : state(State::INITIALIZED), logger(logging.get("web")), context(nullptr),
      vhost(nullptr), port(getuid() > 0 ? 8080 : 80), api(a) {
  lws_set_log_level(lwsLogLevel(logging.getLevel()), lwsLogger);
}

Web::~Web() {
  if (state == State::STARTED)
    stop();
}

int Web::parse(json_t *json) {
  int ret, enabled = 1;
  const char *cert = nullptr;
  const char *pkey = nullptr;
  json_error_t err;

  ret = json_unpack_ex(
      json, &err, JSON_STRICT, "{ s?: s, s?: s, s?: i, s?: b }", "ssl_cert",
      &cert, "ssl_private_key", &pkey, "port", &port, "enabled", &enabled);
  if (ret)
    throw ConfigError(json, err, "node-config-http");

  if (cert)
    ssl_cert = cert;

  if (pkey)
    ssl_private_key = pkey;

  if (!enabled)
    port = CONTEXT_PORT_NO_LISTEN;

  state = State::PARSED;

  return 0;
}

void Web::start() {
  // Start server
  lws_context_creation_info ctx_info;

  memset(&ctx_info, 0, sizeof(ctx_info));

  ctx_info.port = port;
  ctx_info.protocols = protocols;
#ifndef LWS_WITHOUT_EXTENSIONS
  ctx_info.extensions = extensions;
#endif
  ctx_info.ssl_cert_filepath = ssl_cert.empty() ? nullptr : ssl_cert.c_str();
  ctx_info.ssl_private_key_filepath =
      ssl_private_key.empty() ? nullptr : ssl_private_key.c_str();
  ctx_info.gid = -1;
  ctx_info.uid = -1;
  ctx_info.options =
      LWS_SERVER_OPTION_EXPLICIT_VHOSTS | LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  ctx_info.user = (void *)this;
#if LWS_LIBRARY_VERSION_NUMBER <= 3000000
  // See: https://github.com/warmcat/libwebsockets/issues/1249
  ctx_info.max_http_header_pool = 1024;
#endif
  ctx_info.mounts = mounts;

  logger->info("Starting sub-system");

  context = lws_create_context(&ctx_info);
  if (context == nullptr)
    throw RuntimeError("Failed to initialize server context");

  for (int tries = 10; tries > 0; tries--) {
    vhost = lws_create_vhost(context, &ctx_info);
    if (vhost)
      break;

    ctx_info.port++;
    logger->warn("Failed to setup vhost. Trying another port: {}",
                 ctx_info.port);
  }

  if (vhost == nullptr)
    throw RuntimeError("Failed to initialize virtual host");

  // Start thread
  running = true;
  thread = std::thread(&Web::worker, this);

  state = State::STARTED;
}

void Web::stop() {
  assert(state == State::STARTED);

  logger->info("Stopping sub-system");

  running = false;
  lws_cancel_service(context);
  thread.join();

  lws_context_destroy(context);

  state = State::STOPPED;
}

void Web::callbackOnWritable(lws *wsi) {
  writables.push(wsi);
  lws_cancel_service(context);
}
