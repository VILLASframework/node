/* Node type: amqp.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/node/config.hpp>

#ifdef RABBITMQ_C_NEW_INCLUDE_DIR
#include <rabbitmq-c/amqp.h>
#else
#include <amqp.h>
#endif

#include <villas/format.hpp>
#include <villas/list.hpp>

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;

struct amqp_ssl_info {
  int verify_peer;
  int verify_hostname;
  char *ca_cert;
  char *client_cert;
  char *client_key;
};

struct amqp {
  char *uri;

  struct amqp_connection_info connection_info;
  struct amqp_ssl_info ssl_info;

  amqp_bytes_t routing_key;
  amqp_bytes_t exchange;

  // We need to create two connection because rabbitmq-c is not thread-safe!
  amqp_connection_state_t producer;
  amqp_connection_state_t consumer;

  Format *formatter;
};

char *amqp_print(NodeCompat *n);

int amqp_parse(NodeCompat *n, json_t *json);

int amqp_start(NodeCompat *n);

int amqp_init(NodeCompat *n);

int amqp_destroy(NodeCompat *n);

int amqp_poll_fds(NodeCompat *n, int fds[]);

int amqp_stop(NodeCompat *n);

int amqp_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int amqp_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

} // namespace node
} // namespace villas
