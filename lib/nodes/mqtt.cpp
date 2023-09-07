/* Node type: mqtt.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <cstring>
#include <mosquitto.h>
#include <mutex>

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/mqtt.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static void mqtt_log_cb(struct mosquitto *mosq, void *ctx, int level,
                        const char *str) {
  auto *n = (NodeCompat *)ctx;

  switch (level) {
  case MOSQ_LOG_NONE:
  case MOSQ_LOG_INFO:
  case MOSQ_LOG_NOTICE:
    n->logger->info("{}", str);
    break;

  case MOSQ_LOG_WARNING:
    n->logger->warn("{}", str);
    break;

  case MOSQ_LOG_ERR:
    n->logger->error("{}", str);
    break;

  case MOSQ_LOG_DEBUG:
    n->logger->debug("{}", str);
    break;
  }
}

static void mqtt_connect_cb(struct mosquitto *mosq, void *ctx, int result) {
  auto *n = (NodeCompat *)ctx;
  auto *m = n->getData<struct mqtt>();

  int ret;

  n->logger->info("Connected to broker {}", m->host);

  if (m->subscribe) {
    ret = mosquitto_subscribe(m->client, nullptr, m->subscribe, m->qos);
    if (ret)
      n->logger->warn("Failed to subscribe to topic '{}': {}", m->subscribe,
                      mosquitto_strerror(ret));
  } else
    n->logger->warn("No subscription as no topic is configured");
}

static void mqtt_disconnect_cb(struct mosquitto *mosq, void *ctx, int result) {
  auto *n = (NodeCompat *)ctx;
  auto *m = n->getData<struct mqtt>();

  n->logger->info("Disconnected from broker {}", m->host);
}

static void mqtt_message_cb(struct mosquitto *mosq, void *ctx,
                            const struct mosquitto_message *msg) {
  int ret;
  auto *n = (NodeCompat *)ctx;
  auto *m = n->getData<struct mqtt>();
  struct Sample *smps[n->in.vectorize];

  n->logger->debug("Received a message of {} bytes from broker {}",
                   msg->payloadlen, m->host);

  ret = sample_alloc_many(&m->pool, smps, n->in.vectorize);
  if (ret <= 0) {
    n->logger->warn("Pool underrun in subscriber");
    return;
  }

  ret = m->formatter->sscan((char *)msg->payload, msg->payloadlen, nullptr,
                            smps, n->in.vectorize);
  if (ret < 0) {
    n->logger->warn("Received an invalid message");
    n->logger->warn("  Payload: {}", (char *)msg->payload);
    return;
  }

  if (ret == 0) {
    n->logger->debug("Skip empty message");
    sample_decref_many(smps, n->in.vectorize);
    return;
  }

  ret = queue_signalled_push_many(&m->queue, (void **)smps, n->in.vectorize);
  if (ret < (int)n->in.vectorize)
    n->logger->warn("Failed to enqueue samples");
}

static void mqtt_subscribe_cb(struct mosquitto *mosq, void *ctx, int mid,
                              int qos_count, const int *granted_qos) {
  auto *n = (NodeCompat *)ctx;
  auto *m = n->getData<struct mqtt>();

  n->logger->info("Subscribed to broker {}", m->host);
}

int villas::node::mqtt_reverse(NodeCompat *n) {
  auto *m = n->getData<struct mqtt>();

  SWAP(m->publish, m->subscribe);

  return 0;
}

int villas::node::mqtt_init(NodeCompat *n) {
  auto *m = n->getData<struct mqtt>();

  m->client = mosquitto_new(nullptr, true, (void *)n);
  if (!m->client)
    return -1;

  mosquitto_log_callback_set(m->client, mqtt_log_cb);
  mosquitto_connect_callback_set(m->client, mqtt_connect_cb);
  mosquitto_disconnect_callback_set(m->client, mqtt_disconnect_cb);
  mosquitto_message_callback_set(m->client, mqtt_message_cb);
  mosquitto_subscribe_callback_set(m->client, mqtt_subscribe_cb);

  m->formatter = nullptr;

  // Default values
  m->port = 1883;
  m->qos = 0;
  m->retain = 0;
  m->keepalive = 5; // 5 second, minimum required for libmosquitto

  m->host = nullptr;
  m->username = nullptr;
  m->password = nullptr;
  m->publish = nullptr;
  m->subscribe = nullptr;

  m->ssl.enabled = 0;
  m->ssl.insecure = 0;
  m->ssl.cafile = nullptr;
  m->ssl.capath = nullptr;
  m->ssl.certfile = nullptr;
  m->ssl.keyfile = nullptr;
  m->ssl.cert_reqs = SSL_VERIFY_PEER;
  m->ssl.tls_version = nullptr;
  m->ssl.ciphers = nullptr;

  return 0;
}

int villas::node::mqtt_parse(NodeCompat *n, json_t *json) {
  int ret;
  auto *m = n->getData<struct mqtt>();

  const char *host;
  const char *publish = nullptr;
  const char *subscribe = nullptr;
  const char *username = nullptr;
  const char *password = nullptr;

  json_error_t err;
  json_t *json_ssl = nullptr;
  json_t *json_format = nullptr;

  ret = json_unpack_ex(json, &err, 0,
                       "{ s?: { s?: s }, s?: { s?: s }, s?: o, s: s, s?: i, "
                       "s?: i, s?: i, s?: b, s?: s, s?: s, s?: o }",
                       "out", "publish", &publish, "in", "subscribe",
                       &subscribe, "format", &json_format, "host", &host,
                       "port", &m->port, "qos", &m->qos, "keepalive",
                       &m->keepalive, "retain", &m->retain, "username",
                       &username, "password", &password, "ssl", &json_ssl);
  if (ret)
    throw ConfigError(json, err, "node-config-node-mqtt");

  m->host = strdup(host);
  m->publish = publish ? strdup(publish) : nullptr;
  m->subscribe = subscribe ? strdup(subscribe) : nullptr;
  m->username = username ? strdup(username) : nullptr;
  m->password = password ? strdup(password) : nullptr;

  if (!m->publish && !m->subscribe)
    throw ConfigError(json, "node-config-node-mqtt",
                      "At least one topic has to be specified for node {}",
                      n->getName());

  if (json_ssl) {
    m->ssl.enabled = 1;

    const char *cafile = nullptr;
    const char *capath = nullptr;
    const char *certfile = nullptr;
    const char *keyfile = nullptr;
    const char *tls_version = nullptr;
    const char *ciphers = nullptr;

    ret = json_unpack_ex(
        json_ssl, &err, 0,
        "{ s?: b, s?: b, s?: s, s?: s, s?: s, s?: s, s?: s, s?: b, s?: s }",
        "enabled", &m->ssl.enabled, "insecure", &m->ssl.insecure, "cafile",
        &cafile, "capath", &capath, "certfile", &certfile, "keyfile", &keyfile,
        "cipher", &ciphers, "verify", &m->ssl.cert_reqs, "tls_version",
        &tls_version);
    if (ret)
      throw ConfigError(json_ssl, err, "node-config-node-mqtt-ssl",
                        "Failed to parse SSL configuration of node {}",
                        n->getName());

    if (m->ssl.enabled && !cafile && !capath)
      throw ConfigError(json_ssl, "node-config-node-mqtt-ssl",
                        "Either 'ssl.cafile' or 'ssl.capath' settings must be "
                        "set for node {}.",
                        n->getName());

    m->ssl.cafile = cafile ? strdup(cafile) : nullptr;
    m->ssl.capath = capath ? strdup(capath) : nullptr;
    m->ssl.certfile = certfile ? strdup(certfile) : nullptr;
    m->ssl.keyfile = keyfile ? strdup(keyfile) : nullptr;
    m->ssl.ciphers = ciphers ? strdup(ciphers) : nullptr;
  }

  // Format
  if (m->formatter)
    delete m->formatter;
  m->formatter = json_format ? FormatFactory::make(json_format)
                             : FormatFactory::make("json");
  if (!m->formatter)
    throw ConfigError(json_format, "node-config-node-mqtt-format",
                      "Invalid format configuration");

  return 0;
}

int villas::node::mqtt_check(NodeCompat *n) {
  int ret;
  auto *m = n->getData<struct mqtt>();

  ret = mosquitto_sub_topic_check(m->subscribe);
  if (ret != MOSQ_ERR_SUCCESS)
    throw RuntimeError("Invalid subscribe topic: '{}': {}", m->subscribe,
                       mosquitto_strerror(ret));

  ret = mosquitto_pub_topic_check(m->publish);
  if (ret != MOSQ_ERR_SUCCESS)
    throw RuntimeError("Invalid publish topic: '{}': {}", m->publish,
                       mosquitto_strerror(ret));

  return 0;
}

int villas::node::mqtt_prepare(NodeCompat *n) {
  int ret;
  auto *m = n->getData<struct mqtt>();

  m->formatter->start(n->getInputSignals(false), ~(int)SampleFlags::HAS_OFFSET);

  ret = pool_init(&m->pool, 1024,
                  SAMPLE_LENGTH(n->getInputSignals(false)->size()));
  if (ret)
    return ret;

  ret = queue_signalled_init(&m->queue, 1024);
  if (ret)
    return ret;

  return 0;
}

char *villas::node::mqtt_print(NodeCompat *n) {
  auto *m = n->getData<struct mqtt>();

  char *buf = nullptr;

  strcatf(&buf, "host=%s, port=%d, keepalive=%d, ssl=%s", m->host, m->port,
          m->keepalive, m->ssl.enabled ? "yes" : "no");

  // Only show if not default
  if (m->username)
    strcatf(&buf, ", username=%s", m->username);

  if (m->publish)
    strcatf(&buf, ", out.publish=%s", m->publish);

  if (m->subscribe)
    strcatf(&buf, ", in.subscribe=%s", m->subscribe);

  return buf;
}

int villas::node::mqtt_destroy(NodeCompat *n) {
  int ret;
  auto *m = n->getData<struct mqtt>();

  mosquitto_destroy(m->client);

  ret = pool_destroy(&m->pool);
  if (ret)
    return ret;

  ret = queue_signalled_destroy(&m->queue);
  if (ret)
    return ret;

  if (m->publish)
    free(m->publish);

  if (m->subscribe)
    free(m->subscribe);

  if (m->password)
    free(m->password);

  if (m->username)
    free(m->username);

  if (m->host)
    free(m->host);

  if (m->formatter)
    delete m->formatter;

  return 0;
}

int villas::node::mqtt_start(NodeCompat *n) {
  int ret;
  auto *m = n->getData<struct mqtt>();

  if (m->username && m->password) {
    ret = mosquitto_username_pw_set(m->client, m->username, m->password);
    if (ret != MOSQ_ERR_SUCCESS)
      goto mosquitto_error;
  }

  if (m->ssl.enabled) {
    ret = mosquitto_tls_set(m->client, m->ssl.cafile, m->ssl.capath,
                            m->ssl.certfile, m->ssl.keyfile, nullptr);
    if (ret != MOSQ_ERR_SUCCESS)
      goto mosquitto_error;

    ret = mosquitto_tls_insecure_set(m->client, m->ssl.insecure);
    if (ret != MOSQ_ERR_SUCCESS)
      goto mosquitto_error;

    ret = mosquitto_tls_opts_set(m->client, m->ssl.cert_reqs,
                                 m->ssl.tls_version, m->ssl.ciphers);
    if (ret != MOSQ_ERR_SUCCESS)
      goto mosquitto_error;
  }

  ret = mosquitto_connect(m->client, m->host, m->port, m->keepalive);
  if (ret != MOSQ_ERR_SUCCESS)
    goto mosquitto_error;

  ret = mosquitto_loop_start(m->client);
  if (ret != MOSQ_ERR_SUCCESS)
    goto mosquitto_error;

  return 0;

mosquitto_error:
  n->logger->warn("{}", mosquitto_strerror(ret));

  return ret;
}

int villas::node::mqtt_stop(NodeCompat *n) {
  int ret;
  auto *m = n->getData<struct mqtt>();

  ret = mosquitto_disconnect(m->client);
  if (ret != MOSQ_ERR_SUCCESS)
    goto mosquitto_error;

  ret = mosquitto_loop_stop(m->client, false);
  if (ret != MOSQ_ERR_SUCCESS)
    goto mosquitto_error;

  ret = queue_signalled_close(&m->queue);
  if (ret)
    return ret;

  return 0;

mosquitto_error:
  n->logger->warn("{}", mosquitto_strerror(ret));

  return ret;
}

int villas::node::mqtt_type_start(villas::node::SuperNode *sn) {
  int ret;

  ret = mosquitto_lib_init();
  if (ret != MOSQ_ERR_SUCCESS)
    goto mosquitto_error;

  return 0;

mosquitto_error:
  auto logger = logging.get("node:mqtt");
  logger->warn("{}", mosquitto_strerror(ret));

  return ret;
}

int villas::node::mqtt_type_stop() {
  int ret;

  ret = mosquitto_lib_cleanup();
  if (ret != MOSQ_ERR_SUCCESS)
    goto mosquitto_error;

  return 0;

mosquitto_error:
  auto logger = logging.get("node:mqtt");
  logger->warn("{}", mosquitto_strerror(ret));

  return ret;
}

int villas::node::mqtt_read(NodeCompat *n, struct Sample *const smps[],
                            unsigned cnt) {
  int pulled;
  auto *m = n->getData<struct mqtt>();
  struct Sample *smpt[cnt];

  pulled = queue_signalled_pull_many(&m->queue, (void **)smpt, cnt);

  sample_copy_many(smps, smpt, pulled);
  sample_decref_many(smpt, pulled);

  return pulled;
}

int villas::node::mqtt_write(NodeCompat *n, struct Sample *const smps[],
                             unsigned cnt) {
  int ret;
  auto *m = n->getData<struct mqtt>();

  size_t wbytes;

  char data[1500];

  ret = m->formatter->sprint(data, sizeof(data), &wbytes, smps, cnt);
  if (ret < 0)
    return ret;

  if (m->publish) {
    ret = mosquitto_publish(m->client, nullptr /* mid */, m->publish, wbytes,
                            data, m->qos, m->retain);
    if (ret != MOSQ_ERR_SUCCESS) {
      n->logger->warn("Publish failed: {}", mosquitto_strerror(ret));
      return -abs(ret);
    }
  } else
    n->logger->warn(
        "No publish possible because no publish topic is configured");

  return cnt;
}

int villas::node::mqtt_poll_fds(NodeCompat *n, int fds[]) {
  auto *m = n->getData<struct mqtt>();

  fds[0] = queue_signalled_fd(&m->queue);

  return 1;
}

static NodeCompatType p;

__attribute__((constructor(110))) static void register_plugin() {
  p.name = "mqtt";
  p.description = "Message Queuing Telemetry Transport (libmosquitto)";
  p.vectorize = 0;
  p.size = sizeof(struct mqtt);
  p.type.start = mqtt_type_start;
  p.type.stop = mqtt_type_stop;
  p.destroy = mqtt_destroy;
  p.prepare = mqtt_prepare;
  p.parse = mqtt_parse;
  p.check = mqtt_check;
  p.prepare = mqtt_prepare;
  p.print = mqtt_print;
  p.init = mqtt_init;
  p.destroy = mqtt_destroy;
  p.start = mqtt_start;
  p.stop = mqtt_stop;
  p.read = mqtt_read;
  p.write = mqtt_write;
  p.reverse = mqtt_reverse;
  p.poll_fds = mqtt_poll_fds;

  static NodeCompatFactory ncp(&p);
}
