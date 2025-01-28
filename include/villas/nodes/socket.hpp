/* The socket node-type for Layer 2, 3, 4 BSD-style sockets.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/format.hpp>
#include <villas/node/config.hpp>
#include <villas/socket_addr.hpp>

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;

// The maximum length of a packet which contains stuct msg.
#define SOCKET_INITIAL_BUFFER_LEN (64 * 1024)

struct Socket {
  int sd; // The socket descriptor
  int clt_sd; // TCP client socket descriptor
  int verify_source; // Verify the source address of incoming packets against socket::remote.
  bool tcp_connect = false; // TCP connection status bit

  enum SocketLayer
      layer; // The OSI / IP layer which should be used for this socket

  Format *formatter;

  // Multicast options
  struct multicast {
    int enabled;         // Is multicast enabled?
    unsigned char loop;  // Loopback multicast packets to local host?
    unsigned char ttl;   // The time to live for multicast packets.
    struct ip_mreq mreq; // A multicast group to join.
  } multicast;

  struct {
    char *buf; // Buffer for receiving messages
    size_t buflen;
    union sockaddr_union saddr; // Remote address of the socket
  } in, out;
};

int socket_type_start(SuperNode *sn);

int socket_type_stop();

int socket_init(NodeCompat *n);

int socket_destroy(NodeCompat *n);

int socket_start(NodeCompat *n);

int socket_check(NodeCompat *n);

int socket_stop(NodeCompat *n);

int socket_reverse(NodeCompat *n);

int socket_fds(NodeCompat *n, int fds[]);

int socket_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int socket_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int socket_parse(NodeCompat *n, json_t *json);

void socket_tcp_connection(NodeCompat *n, Socket *s);

char *socket_print(NodeCompat *n);

} // namespace node
} // namespace villas
