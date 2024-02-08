/* Node type: WebSockets.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fmt/ostream.h>
#include <libwebsockets.h>
#include <villas/buffer.hpp>
#include <villas/common.hpp>
#include <villas/config.hpp>
#include <villas/format.hpp>
#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/node_compat.hpp>
#include <villas/pool.hpp>
#include <villas/queue_signalled.h>

// Forward declarations
struct lws;

namespace villas {
namespace node {

#define DEFAULT_WEBSOCKET_QUEUE_LENGTH (DEFAULT_QUEUE_LENGTH * 64)

// Internal data per websocket node
struct websocket {
  struct List
      destinations; // List of websocket servers connect to in client mode (struct websocket_destination).

  bool wait; // Wait until all destinations are connected.

  struct Pool pool;
  struct CQueueSignalled
      queue; // For samples which are received from WebSockets
};

struct websocket_destination {
  char *uri;
  struct lws_client_connect_info info;
};

// Internal datastructures
struct websocket_connection {
  enum State {
    DESTROYED,
    INITIALIZED,
    CONNECTING,
    RECONNECTING,
    ESTABLISHED,
    CLOSING,
    CLOSED,
    ERROR
  } state; // The current status of this connection.

  enum class Mode {
    CLIENT,
    SERVER,
  } mode;

  struct lws *wsi;
  NodeCompat *node;
  Format *formatter;
  struct CQueue queue; // For samples which are sent to the Websocket

  struct websocket_destination *destination;

  struct {
    villas::Buffer *recv; // A buffer for reconstructing fragmented messages.
    villas::Buffer
        *send; // A buffer for constructing messages before calling lws_write()
  } buffers;

  friend std::ostream &operator<<(std::ostream &os,
                                  const struct websocket_connection &c) {
    if (c.wsi) {
      char name[128];

      lws_get_peer_simple(c.wsi, name, sizeof(name));

      os << "remote=" << name;
    } else if (c.mode == websocket_connection::Mode::CLIENT &&
               c.destination != nullptr)
      os << "dest=" << c.destination->info.address << ":"
         << c.destination->info.port;

    if (c.node)
      os << ", node=" << c.node->getName();

    os << ", mode="
       << (c.mode == websocket_connection::Mode::CLIENT ? "client" : "server");

    return os;
  }

  std::string toString() {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  }
};

int websocket_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason,
                          void *user, void *in, size_t len);

int websocket_type_start(SuperNode *sn);

int websocket_type_stop();

int websocket_parse(NodeCompat *n, json_t *j);

char *websocket_print(NodeCompat *n);

int websocket_start(NodeCompat *n);

int websocket_stop(NodeCompat *n);

int websocket_init(NodeCompat *n);

int websocket_destroy(NodeCompat *n);

int websocket_poll_fds(NodeCompat *n, int fds[]);

int websocket_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int websocket_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

} // namespace node
} // namespace villas

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::node::websocket_connection>
    : public fmt::ostream_formatter {};
#endif
