/* Node type: nanomsg.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>

#include <villas/node/config.hpp>

#ifdef RABBITMQ_C_NEW_INCLUDE_DIR
#include <rabbitmq-c/ssl_socket.h>
#include <rabbitmq-c/tcp_socket.h>
#else
#include <amqp_ssl_socket.h>
#include <amqp_tcp_socket.h>
#endif

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/amqp.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static void amqp_default_ssl_info(struct amqp_ssl_info *s) {
  s->verify_peer = 1;
  s->verify_hostname = 1;
  s->client_key = nullptr;
  s->client_cert = nullptr;
  s->ca_cert = nullptr;
}

static amqp_bytes_t amqp_bytes_strdup(const char *str) {
  size_t len = strlen(str) + 1;
  amqp_bytes_t buf = amqp_bytes_malloc(len);

  memcpy(buf.bytes, str, len);

  return buf;
}

static amqp_connection_state_t amqp_connect(NodeCompat *n,
                                            struct amqp_connection_info *ci,
                                            struct amqp_ssl_info *ssl) {
  int ret;
  amqp_rpc_reply_t rep;
  amqp_connection_state_t conn;
  amqp_socket_t *sock;

  conn = amqp_new_connection();
  if (!conn)
    throw MemoryAllocationError();

  if (ci->ssl) {
    sock = amqp_ssl_socket_new(conn);
    if (!sock)
      throw MemoryAllocationError();

    amqp_ssl_socket_set_verify_peer(sock, ssl->verify_peer);
    amqp_ssl_socket_set_verify_hostname(sock, ssl->verify_hostname);

    if (ssl->ca_cert) {
      ret = amqp_ssl_socket_set_cacert(sock, ssl->ca_cert);
      if (ret) {
        n->logger->error("Failed to set CA cert: {}", amqp_error_string2(ret));
        return nullptr;
      }
    }

    if (ssl->client_key && ssl->client_cert) {
      ret = amqp_ssl_socket_set_key(sock, ssl->client_cert, ssl->client_key);
      if (ret) {
        n->logger->error("Failed to set client cert: {}",
                         amqp_error_string2(ret));
        return nullptr;
      }
    }
  } else {
    sock = amqp_tcp_socket_new(conn);
    if (!sock)
      throw MemoryAllocationError();
  }

  ret = amqp_socket_open(sock, ci->host, ci->port);
  if (ret != AMQP_STATUS_OK) {
    n->logger->error("Failed to open socket: {}", amqp_error_string2(ret));
    return nullptr;
  }

  rep = amqp_login(conn, ci->vhost, 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
                   ci->user, ci->password);
  if (rep.reply_type != AMQP_RESPONSE_NORMAL) {
    n->logger->error("Failed to login");
    return nullptr;
  }

  amqp_channel_open(conn, 1);
  rep = amqp_get_rpc_reply(conn);
  if (rep.reply_type != AMQP_RESPONSE_NORMAL) {
    n->logger->error("Failed to open channel");
    return nullptr;
  }

  return conn;
}

static int amqp_close(NodeCompat *n, amqp_connection_state_t conn) {
  amqp_rpc_reply_t rep;

  rep = amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
  if (rep.reply_type != AMQP_RESPONSE_NORMAL) {
    n->logger->error("Failed to close channel");
    return -1;
  }

  rep = amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
  if (rep.reply_type != AMQP_RESPONSE_NORMAL) {
    n->logger->error("Failed to close connection");
    return -1;
  };

  return 0;
}

int villas::node::amqp_init(NodeCompat *n) {
  auto *a = n->getData<struct amqp>();

  // Default values
  amqp_default_ssl_info(&a->ssl_info);
  amqp_default_connection_info(&a->connection_info);

  a->formatter = nullptr;

  return 0;
}

int villas::node::amqp_parse(NodeCompat *n, json_t *json) {
  int ret;
  auto *a = n->getData<struct amqp>();

  int port = 5672;
  const char *uri = nullptr;
  const char *host = "localhost";
  const char *vhost = "/";
  const char *username = "guest";
  const char *password = "guest";
  const char *exchange, *routing_key;

  json_error_t err;

  json_t *json_ssl = nullptr;
  json_t *json_format = nullptr;

  ret = json_unpack_ex(
      json, &err, 0,
      "{ s?: s, s?: s, s?: s, s?: s, s?: s, s?: i, s: s, s: s, s?: o, s?: o }",
      "uri", &uri, "host", &host, "vhost", &vhost, "username", &username,
      "password", &password, "port", &port, "exchange", &exchange,
      "routing_key", &routing_key, "format", &json_format, "ssl", &json_ssl);
  if (ret)
    throw ConfigError(json, err, "node-config-node-amqp");

  a->exchange = amqp_bytes_strdup(exchange);
  a->routing_key = amqp_bytes_strdup(routing_key);

  if (uri)
    a->uri = strdup(uri);
  else
    a->uri = strf("%s://%s:%s@%s:%d/%s", json_ssl ? "amqps" : "amqp", username,
                  password, host, port, vhost);

  ret = amqp_parse_url(a->uri, &a->connection_info);
  if (ret != AMQP_STATUS_OK)
    throw ConfigError(json, "node-config-node-uri", "Failed to parse URI '{}'",
                      uri);

  if (json_ssl) {
    const char *ca_cert = nullptr;
    const char *client_cert = nullptr;
    const char *client_key = nullptr;

    ret = json_unpack_ex(
        json_ssl, &err, 0, "{ s?: b, s?: b, s?: s, s?: s, s?: s }",
        "verify_peer", &a->ssl_info.verify_peer, "verify_hostname",
        &a->ssl_info.verify_hostname, "ca_cert", &ca_cert, "client_key",
        &client_key, "client_cert", &client_cert);
    if (ret)
      throw ConfigError(json_ssl, err, "node-config-node-amqp-ssl",
                        "Failed to parse SSL configuration");

    if (ca_cert)
      a->ssl_info.ca_cert = strdup(ca_cert);

    if (client_cert)
      a->ssl_info.client_cert = strdup(client_cert);

    if (client_key)
      a->ssl_info.client_key = strdup(client_key);
  }

  // Format
  if (a->formatter)
    delete a->formatter;
  a->formatter = json_format ? FormatFactory::make(json_format)
                             : FormatFactory::make("json");
  if (!a->formatter)
    throw ConfigError(json_format, "node-config-node-amqp-format",
                      "Invalid format configuration");

  return 0;
}

char *villas::node::amqp_print(NodeCompat *n) {
  auto *a = n->getData<struct amqp>();

  char *buf = nullptr;

  strcatf(&buf, "uri=%s://%s:%s@%s:%d%s, exchange=%s, routing_key=%s",
          a->connection_info.ssl ? "amqps" : "amqp", a->connection_info.user,
          a->connection_info.password, a->connection_info.host,
          a->connection_info.port, a->connection_info.vhost,
          (char *)a->exchange.bytes, (char *)a->routing_key.bytes);

  if (a->connection_info.ssl) {
    strcatf(&buf, ", ssl_info.verify_peer=%s, ssl_info.verify_hostname=%s",
            a->ssl_info.verify_peer ? "true" : "false",
            a->ssl_info.verify_hostname ? "true" : "false");

    if (a->ssl_info.ca_cert)
      strcatf(&buf, ", ssl_info.ca_cert=%s", a->ssl_info.ca_cert);

    if (a->ssl_info.client_cert)
      strcatf(&buf, ", ssl_info.client_cert=%s", a->ssl_info.client_cert);

    if (a->ssl_info.client_key)
      strcatf(&buf, ", ssl_info.client_key=%s", a->ssl_info.client_key);
  }

  return buf;
}

int villas::node::amqp_start(NodeCompat *n) {
  auto *a = n->getData<struct amqp>();

  amqp_bytes_t queue;
  amqp_rpc_reply_t rep;
  amqp_queue_declare_ok_t *r;

  a->formatter->start(n->getInputSignals(false), ~(int)SampleFlags::HAS_OFFSET);

  // Connect producer
  a->producer = amqp_connect(n, &a->connection_info, &a->ssl_info);
  if (!a->producer)
    return -1;

  // Connect consumer
  a->consumer = amqp_connect(n, &a->connection_info, &a->ssl_info);
  if (!a->consumer)
    return -1;

  // Declare exchange
  amqp_exchange_declare(a->producer, 1, a->exchange,
                        amqp_cstring_bytes("direct"), 0, 0, 0, 0,
                        amqp_empty_table);
  rep = amqp_get_rpc_reply(a->consumer);
  if (rep.reply_type != AMQP_RESPONSE_NORMAL)
    return -1;

  // Declare private queue
  r = amqp_queue_declare(a->consumer, 1, amqp_empty_bytes, 0, 0, 0, 1,
                         amqp_empty_table);
  rep = amqp_get_rpc_reply(a->consumer);
  if (rep.reply_type != AMQP_RESPONSE_NORMAL)
    return -1;

  queue = amqp_bytes_malloc_dup(r->queue);
  if (queue.bytes == nullptr)
    return -1;

  // Bind queue to exchange
  amqp_queue_bind(a->consumer, 1, queue, a->exchange, a->routing_key,
                  amqp_empty_table);
  rep = amqp_get_rpc_reply(a->consumer);
  if (rep.reply_type != AMQP_RESPONSE_NORMAL)
    return -1;

  // Start consumer
  amqp_basic_consume(a->consumer, 1, queue, amqp_empty_bytes, 0, 1, 0,
                     amqp_empty_table);
  rep = amqp_get_rpc_reply(a->consumer);
  if (rep.reply_type != AMQP_RESPONSE_NORMAL)
    return -1;

  amqp_bytes_free(queue);

  return 0;
}

int villas::node::amqp_stop(NodeCompat *n) {
  int ret;
  auto *a = n->getData<struct amqp>();

  ret = amqp_close(n, a->consumer);
  if (ret)
    return ret;

  ret = amqp_close(n, a->producer);
  if (ret)
    return ret;

  return 0;
}

int villas::node::amqp_read(NodeCompat *n, struct Sample *const smps[],
                            unsigned cnt) {
  int ret;
  auto *a = n->getData<struct amqp>();
  amqp_envelope_t env;
  amqp_rpc_reply_t rep;

  rep = amqp_consume_message(a->consumer, &env, nullptr, 0);
  if (rep.reply_type != AMQP_RESPONSE_NORMAL)
    return -1;

  ret = a->formatter->sscan(static_cast<char *>(env.message.body.bytes),
                            env.message.body.len, nullptr, smps, cnt);

  amqp_destroy_envelope(&env);

  return ret;
}

int villas::node::amqp_write(NodeCompat *n, struct Sample *const smps[],
                             unsigned cnt) {
  int ret;
  auto *a = n->getData<struct amqp>();
  char data[1500];
  size_t wbytes;

  ret = a->formatter->sprint(data, sizeof(data), &wbytes, smps, cnt);
  if (ret <= 0)
    return -1;

  amqp_bytes_t message = {.len = wbytes, .bytes = data};

  // Send message
  ret = amqp_basic_publish(a->producer, 1, a->exchange, a->routing_key, 0, 0,
                           nullptr, message);

  if (ret != AMQP_STATUS_OK)
    return -1;

  return cnt;
}

int villas::node::amqp_poll_fds(NodeCompat *n, int fds[]) {
  auto *a = n->getData<struct amqp>();

  amqp_socket_t *sock = amqp_get_socket(a->consumer);

  fds[0] = amqp_socket_get_sockfd(sock);

  return 1;
}

int villas::node::amqp_destroy(NodeCompat *n) {
  auto *a = n->getData<struct amqp>();

  if (a->uri)
    free(a->uri);

  if (a->ssl_info.client_cert)
    free(a->ssl_info.client_cert);

  if (a->ssl_info.client_key)
    free(a->ssl_info.client_key);

  if (a->ssl_info.ca_cert)
    free(a->ssl_info.ca_cert);

  if (a->producer)
    amqp_destroy_connection(a->producer);

  if (a->consumer)
    amqp_destroy_connection(a->consumer);

  if (a->formatter)
    delete a->formatter;

  return 0;
}

static NodeCompatType p;

__attribute__((constructor(110))) static void register_plugin() {
  p.name = "amqp";
  p.description = "Advanced Message Queueing Protoocl (rabbitmq-c)";
  p.vectorize = 0;
  p.size = sizeof(struct amqp);
  p.init = amqp_init;
  p.destroy = amqp_destroy;
  p.parse = amqp_parse;
  p.print = amqp_print;
  p.start = amqp_start;
  p.stop = amqp_stop;
  p.read = amqp_read;
  p.write = amqp_write;
  p.poll_fds = amqp_poll_fds;

  static NodeCompatFactory ncp(&p);
}
