/* Node type: ZeroMQ.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>

#include <jansson.h>

#include <villas/format.hpp>
#include <villas/list.hpp>
#include <villas/super_node.hpp>

#if ZMQ_BUILD_DRAFT_API &&                                                     \
    (ZMQ_VERSION_MAJOR > 4 ||                                                  \
     (ZMQ_VERSION_MAJOR == 4 && ZMQ_VERSION_MINOR >= 2))
#define ZMQ_BUILD_DISH 1
#endif

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;
struct Sample;

struct zeromq {
  int ipv6;

  Format *formatter;

  struct Curve {
    int enabled;
    struct {
      char public_key[41];
      char secret_key[41];
    } server, client;
  } curve;

  enum class Pattern {
    PUBSUB,
#ifdef ZMQ_BUILD_DISH
    RADIODISH
#endif
  } pattern;

  struct Dir {
    void *socket; // ZeroMQ socket.
    void *mon_socket;
    struct List endpoints;
    char *filter;
    int bind, pending;
  } in, out;
};

char *zeromq_print(NodeCompat *n);

int zeromq_parse(NodeCompat *n, json_t *json);

int zeromq_type_start(SuperNode *sn);

int zeromq_type_stop();

int zeromq_init(NodeCompat *n);

int zeromq_destroy(NodeCompat *n);

int zeromq_check(NodeCompat *n);

int zeromq_start(NodeCompat *n);

int zeromq_stop(NodeCompat *n);

int zeromq_reverse(NodeCompat *n);

int zeromq_poll_fds(NodeCompat *n, int fds[]);

int zeromq_netem_fds(NodeCompat *n, int fds[]);

int zeromq_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int zeromq_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

} // namespace node
} // namespace villas
