/* Node type: nanomsg.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/nanomsg.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int villas::node::nanomsg_init(NodeCompat *n) {
  auto *m = n->getData<struct nanomsg>();

  m->formatter = nullptr;

  return 0;
}

int villas::node::nanomsg_destroy(NodeCompat *n) {
  auto *m = n->getData<struct nanomsg>();

  if (m->formatter)
    delete m->formatter;

  return 0;
}

int villas::node::nanomsg_reverse(NodeCompat *n) {
  auto *m = n->getData<struct nanomsg>();

  if (list_length(&m->out.endpoints) != 1 || list_length(&m->in.endpoints) != 1)
    return -1;

  char *subscriber = (char *)list_first(&m->in.endpoints);
  char *publisher = (char *)list_first(&m->out.endpoints);

  list_set(&m->in.endpoints, 0, publisher);
  list_set(&m->out.endpoints, 0, subscriber);

  return 0;
}

static int nanomsg_parse_endpoints(struct List *l, json_t *json) {
  const char *ep;

  size_t i;
  json_t *json_val;

  switch (json_typeof(json)) {
  case JSON_ARRAY:
    json_array_foreach(json, i, json_val) {
      ep = json_string_value(json_val);
      if (!ep)
        return -1;

      list_push(l, strdup(ep));
    }
    break;

  case JSON_STRING:
    ep = json_string_value(json);

    list_push(l, strdup(ep));
    break;

  default:
    return -1;
  }

  return 0;
}

int villas::node::nanomsg_parse(NodeCompat *n, json_t *json) {
  int ret;
  auto *m = n->getData<struct nanomsg>();

  json_error_t err;
  json_t *json_format = nullptr;
  json_t *json_out_endpoints = nullptr;
  json_t *json_in_endpoints = nullptr;

  ret = list_init(&m->out.endpoints);
  if (ret)
    return ret;

  ret = list_init(&m->in.endpoints);
  if (ret)
    return ret;

  ret = json_unpack_ex(json, &err, 0, "{ s?: o, s?: { s?: o }, s?: { s?: o } }",
                       "format", &json_format, "out", "endpoints",
                       &json_out_endpoints, "in", "endpoints",
                       &json_in_endpoints);
  if (ret)
    throw ConfigError(json, err, "node-config-node-nanomsg");

  if (json_out_endpoints) {
    ret = nanomsg_parse_endpoints(&m->out.endpoints, json_out_endpoints);
    if (ret < 0)
      throw RuntimeError("Invalid type for 'publish' setting");
  }

  if (json_in_endpoints) {
    ret = nanomsg_parse_endpoints(&m->in.endpoints, json_in_endpoints);
    if (ret < 0)
      throw RuntimeError("Invalid type for 'subscribe' setting");
  }

  // Format
  if (m->formatter)
    delete m->formatter;
  m->formatter = json_format ? FormatFactory::make(json_format)
                             : FormatFactory::make("json");
  if (!m->formatter)
    throw ConfigError(json_format, "node-config-node-nanomsg-format",
                      "Invalid format configuration");

  return 0;
}

char *villas::node::nanomsg_print(NodeCompat *n) {
  auto *m = n->getData<struct nanomsg>();

  char *buf = nullptr;

  strcatf(&buf, "in.endpoints=[ ");

  for (size_t i = 0; i < list_length(&m->in.endpoints); i++) {
    char *ep = (char *)list_at(&m->in.endpoints, i);

    strcatf(&buf, "%s ", ep);
  }

  strcatf(&buf, "], out.endpoints=[ ");

  for (size_t i = 0; i < list_length(&m->out.endpoints); i++) {
    char *ep = (char *)list_at(&m->out.endpoints, i);

    strcatf(&buf, "%s ", ep);
  }

  strcatf(&buf, "]");

  return buf;
}

int villas::node::nanomsg_start(NodeCompat *n) {
  int ret;
  auto *m = n->getData<struct nanomsg>();

  m->formatter->start(n->getInputSignals(false), ~(int)SampleFlags::HAS_OFFSET);

  ret = m->in.socket = nn_socket(AF_SP, NN_SUB);
  if (ret < 0)
    throw RuntimeError("Failed to create nanomsg socket: {}",
                       nn_strerror(errno));

  ret = m->out.socket = nn_socket(AF_SP, NN_PUB);
  if (ret < 0)
    throw RuntimeError("Failed to create nanomsg socket: {}",
                       nn_strerror(errno));

  // Subscribe to all topics
  ret = nn_setsockopt(ret = m->in.socket, NN_SUB, NN_SUB_SUBSCRIBE, "", 0);
  if (ret < 0)
    return ret;

  // Bind publisher to socket
  for (size_t i = 0; i < list_length(&m->out.endpoints); i++) {
    char *ep = (char *)list_at(&m->out.endpoints, i);

    ret = nn_bind(m->out.socket, ep);
    if (ret < 0)
      throw RuntimeError("Failed to connect nanomsg socket to endpoint {}: {}",
                         ep, nn_strerror(errno));
  }

  // Connect subscribers socket
  for (size_t i = 0; i < list_length(&m->in.endpoints); i++) {
    char *ep = (char *)list_at(&m->in.endpoints, i);

    ret = nn_connect(m->in.socket, ep);
    if (ret < 0)
      throw RuntimeError("Failed to connect nanomsg socket to endpoint {}: {}",
                         ep, nn_strerror(errno));
  }

  return 0;
}

int villas::node::nanomsg_stop(NodeCompat *n) {
  int ret;
  auto *m = n->getData<struct nanomsg>();

  ret = nn_close(m->in.socket);
  if (ret < 0)
    return ret;

  ret = nn_close(m->out.socket);
  if (ret < 0)
    return ret;

  return 0;
}

int villas::node::nanomsg_type_stop() {
  nn_term();

  return 0;
}

int villas::node::nanomsg_read(NodeCompat *n, struct Sample *const smps[],
                               unsigned cnt) {
  auto *m = n->getData<struct nanomsg>();
  int bytes;
  char data[NANOMSG_MAX_PACKET_LEN];

  // Receive payload
  bytes = nn_recv(m->in.socket, data, sizeof(data), 0);
  if (bytes < 0)
    return -1;

  return m->formatter->sscan(data, bytes, nullptr, smps, cnt);
}

int villas::node::nanomsg_write(NodeCompat *n, struct Sample *const smps[],
                                unsigned cnt) {
  int ret;
  auto *m = n->getData<struct nanomsg>();

  size_t wbytes;

  char data[NANOMSG_MAX_PACKET_LEN];

  ret = m->formatter->sprint(data, sizeof(data), &wbytes, smps, cnt);
  if (ret <= 0)
    return -1;

  ret = nn_send(m->out.socket, data, wbytes, 0);
  if (ret < 0)
    return ret;

  return cnt;
}

int villas::node::nanomsg_poll_fds(NodeCompat *n, int fds[]) {
  int ret;
  auto *m = n->getData<struct nanomsg>();

  int fd;
  size_t len = sizeof(fd);

  ret = nn_getsockopt(m->in.socket, NN_SOL_SOCKET, NN_RCVFD, &fd, &len);
  if (ret)
    return ret;

  fds[0] = fd;

  return 1;
}

int villas::node::nanomsg_netem_fds(NodeCompat *n, int fds[]) {
  auto *m = n->getData<struct nanomsg>();

  fds[0] = m->out.socket;

  return 1;
}

static NodeCompatType p;

__attribute__((constructor(110))) static void register_plugin() {
  p.name = "nanomsg";
  p.description = "scalability protocols library (libnanomsg)";
  p.vectorize = 0;
  p.size = sizeof(struct nanomsg);
  p.type.stop = nanomsg_type_stop;
  p.init = nanomsg_init;
  p.destroy = nanomsg_destroy;
  p.parse = nanomsg_parse;
  p.print = nanomsg_print;
  p.start = nanomsg_start;
  p.stop = nanomsg_stop;
  p.read = nanomsg_read;
  p.write = nanomsg_write;
  p.reverse = nanomsg_reverse;
  p.poll_fds = nanomsg_poll_fds;
  p.netem_fds = nanomsg_netem_fds;

  static NodeCompatFactory ncp(&p);
}
