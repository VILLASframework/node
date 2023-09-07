/* Node type: kafka.
 *
 * Author: Juan Pablo Noreña <jpnorenam@unal.edu.co>
 * SPDX-FileCopyrightText: 2021 Universidad Nacional de Colombia
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>
#include <librdkafka/rdkafkacpp.h>
#include <sys/syslog.h>

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/kafka.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

// Each process has a list of clients for which a thread invokes the kafka loop
static struct List clients;
static pthread_t thread;
static Logger logger;

static void kafka_logger_cb(const rd_kafka_t *rk, int level, const char *fac,
                            const char *buf) {

  switch (level) {
  case LOG_EMERG:
  case LOG_CRIT:
  case LOG_ERR:
    logger->error("{}: {}", fac, buf);
    break;

  case LOG_ALERT:
  case LOG_WARNING:
    logger->warn("{}: {}", fac, buf);
    break;

  case LOG_DEBUG:
    logger->debug("{}: {}", fac, buf);
    break;

  case LOG_NOTICE:
  case LOG_INFO:
  default:
    logger->info("{}: {}", fac, buf);
    break;
  }
}

static void kafka_message_cb(void *ctx, const rd_kafka_message_t *msg) {
  int ret;
  auto *n = (NodeCompat *)ctx;
  auto *k = n->getData<struct kafka>();
  struct Sample *smps[n->in.vectorize];

  n->logger->debug("Received a message of {} bytes from broker {}", msg->len,
                   k->server);

  ret = sample_alloc_many(&k->pool, smps, n->in.vectorize);
  if (ret <= 0) {
    n->logger->warn("Pool underrun in consumer");
    return;
  }

  ret = k->formatter->sscan((char *)msg->payload, msg->len, nullptr, smps,
                            n->in.vectorize);
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

  ret = queue_signalled_push_many(&k->queue, (void **)smps, n->in.vectorize);
  if (ret < (int)n->in.vectorize)
    n->logger->warn("Failed to enqueue samples");
}

static void *kafka_loop_thread(void *ctx) {
  int ret;

  // Set the cancel type of this thread to async
  ret = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, nullptr);
  if (ret != 0)
    throw RuntimeError("Unable to set cancel type of Kafka communication "
                       "thread to asynchronous.");

  while (true) {
    for (unsigned i = 0; i < list_length(&clients); i++) {
      auto *n = (NodeCompat *)list_at(&clients, i);
      auto *k = n->getData<struct kafka>();

      // Execute kafka loop for this client
      if (k->consumer.client) {
        rd_kafka_message_t *msg =
            rd_kafka_consumer_poll(k->consumer.client, k->timeout * 1000);
        if (msg) {
          kafka_message_cb((void *)n, msg);
          rd_kafka_message_destroy(msg);
        }
      }
    }
  }

  return nullptr;
}

int villas::node::kafka_reverse(NodeCompat *n) {
  auto *k = n->getData<struct kafka>();

  SWAP(k->produce, k->consume);

  return 0;
}

int villas::node::kafka_init(NodeCompat *n) {
  auto *k = n->getData<struct kafka>();

  // Default values
  k->server = nullptr;
  k->protocol = nullptr;
  k->produce = nullptr;
  k->consume = nullptr;
  k->client_id = nullptr;
  k->timeout = 1.0;

  k->consumer.client = nullptr;
  k->consumer.group_id = nullptr;
  k->producer.client = nullptr;
  k->producer.topic = nullptr;

  k->sasl.mechanisms = nullptr;
  k->sasl.username = nullptr;
  k->sasl.password = nullptr;

  k->ssl.ca = nullptr;

  k->formatter = nullptr;

  return 0;
}

int villas::node::kafka_parse(NodeCompat *n, json_t *json) {
  int ret;
  auto *k = n->getData<struct kafka>();

  const char *server;
  const char *produce = nullptr;
  const char *consume = nullptr;
  const char *protocol;
  const char *client_id = "villas-node";
  const char *group_id = nullptr;

  json_error_t err;
  json_t *json_ssl = nullptr;
  json_t *json_sasl = nullptr;
  json_t *json_format = nullptr;

  ret = json_unpack_ex(json, &err, 0,
                       "{ s?: { s?: s }, s?: { s?: s, s?: s }, s?: o, s: s, "
                       "s?: F, s: s, s?: s, s?: o, s?: o }",
                       "out", "produce", &produce, "in", "consume", &consume,
                       "group_id", &group_id, "format", &json_format, "server",
                       &server, "timeout", &k->timeout, "protocol", &protocol,
                       "client_id", &client_id, "ssl", &json_ssl, "sasl",
                       &json_sasl);
  if (ret)
    throw ConfigError(json, err, "node-config-node-kafka");

  k->server = strdup(server);
  k->produce = produce ? strdup(produce) : nullptr;
  k->consume = consume ? strdup(consume) : nullptr;
  k->protocol = strdup(protocol);
  k->client_id = strdup(client_id);
  k->consumer.group_id = group_id ? strdup(group_id) : nullptr;

  if (strcmp(protocol, "SSL") && strcmp(protocol, "PLAINTEXT") &&
      strcmp(protocol, "SASL_SSL") && strcmp(protocol, "SASL_PLAINTEXT"))
    throw ConfigError(json, "node-config-node-kafka-protocol",
                      "Invalid security protocol: {}", protocol);

  if (!k->produce && !k->consume)
    throw ConfigError(json, "node-config-node-kafka",
                      "At least one topic has to be specified for node {}",
                      n->getName());

  if (json_ssl) {
    const char *ca;

    ret = json_unpack_ex(json_ssl, &err, 0, "{ s: s }", "ca", &ca);
    if (ret)
      throw ConfigError(json_ssl, err, "node-config-node-kafka-ssl",
                        "Failed to parse SSL configuration of node {}",
                        n->getName());

    k->ssl.ca = strdup(ca);
  }

  if (json_sasl) {
    const char *mechanisms;
    const char *username;
    const char *password;

    ret = json_unpack_ex(json_sasl, &err, 0, "{ s: s, s: s, s: s }",
                         "mechanisms", &mechanisms, "username", &username,
                         "password", &password);
    if (ret)
      throw ConfigError(json_sasl, err, "node-config-node-kafka-sasl",
                        "Failed to parse SASL configuration");

    k->sasl.mechanisms = strdup(mechanisms);
    k->sasl.username = strdup(username);
    k->sasl.password = strdup(password);
  }

  // Format
  if (k->formatter)
    delete k->formatter;
  k->formatter = json_format ? FormatFactory::make(json_format)
                             : FormatFactory::make("villas.binary");
  if (!k->formatter)
    throw ConfigError(json_format, "node-config-node-kafka-format",
                      "Invalid format configuration");

  return 0;
}

int villas::node::kafka_prepare(NodeCompat *n) {
  int ret;
  auto *k = n->getData<struct kafka>();

  k->formatter->start(n->getInputSignals(false), ~(int)SampleFlags::HAS_OFFSET);

  ret = pool_init(&k->pool, 1024,
                  SAMPLE_LENGTH(n->getInputSignals(false)->size()));
  if (ret)
    return ret;

  ret = queue_signalled_init(&k->queue, 1024);
  if (ret)
    return ret;

  return 0;
}

char *villas::node::kafka_print(NodeCompat *n) {
  auto *k = n->getData<struct kafka>();

  char *buf = nullptr;

  strcatf(&buf, "bootstrap.server=%s, client.id=%s, security.protocol=%s",
          k->server, k->client_id, k->protocol);

  // Only show if not default
  if (k->produce)
    strcatf(&buf, ", out.produce=%s", k->produce);

  if (k->consume)
    strcatf(&buf, ", in.consume=%s", k->consume);

  return buf;
}

int villas::node::kafka_destroy(NodeCompat *n) {
  int ret;
  auto *k = n->getData<struct kafka>();

  if (k->producer.client)
    rd_kafka_destroy(k->producer.client);

  if (k->consumer.client)
    rd_kafka_destroy(k->consumer.client);

  if (k->formatter)
    delete k->formatter;

  ret = pool_destroy(&k->pool);
  if (ret)
    return ret;

  ret = queue_signalled_destroy(&k->queue);
  if (ret)
    return ret;

  if (k->produce)
    free(k->produce);

  if (k->consume)
    free(k->consume);

  if (k->protocol)
    free(k->protocol);

  if (k->client_id)
    free(k->client_id);

  free(k->server);

  return 0;
}

int villas::node::kafka_start(NodeCompat *n) {
  int ret;
  char errstr[1024];
  auto *k = n->getData<struct kafka>();

  rd_kafka_conf_t *rdkconf = rd_kafka_conf_new();
  if (!rdkconf)
    throw MemoryAllocationError();

  rd_kafka_conf_set_log_cb(rdkconf, kafka_logger_cb);

  ret = rd_kafka_conf_set(rdkconf, "client.id", k->client_id, errstr,
                          sizeof(errstr));
  if (ret != RD_KAFKA_CONF_OK)
    goto kafka_config_error;

  ret = rd_kafka_conf_set(rdkconf, "bootstrap.servers", k->server, errstr,
                          sizeof(errstr));
  if (ret != RD_KAFKA_CONF_OK)
    goto kafka_config_error;

  ret = rd_kafka_conf_set(rdkconf, "security.protocol", k->protocol, errstr,
                          sizeof(errstr));
  if (ret != RD_KAFKA_CONF_OK)
    goto kafka_config_error;

  if (!strcmp(k->protocol, "SASL_SSL") || !strcmp(k->protocol, "SSL")) {
    ret = rd_kafka_conf_set(rdkconf, "ssl.ca.location", k->ssl.ca, errstr,
                            sizeof(errstr));
    if (ret != RD_KAFKA_CONF_OK)
      goto kafka_config_error;
  }

  if (!strcmp(k->protocol, "SASL_PLAINTEXT") ||
      !strcmp(k->protocol, "SASL_SSL")) {
    ret = rd_kafka_conf_set(rdkconf, "sasl.mechanisms", k->sasl.mechanisms,
                            errstr, sizeof(errstr));
    if (ret != RD_KAFKA_CONF_OK)
      goto kafka_config_error;

    ret = rd_kafka_conf_set(rdkconf, "sasl.username", k->sasl.username, errstr,
                            sizeof(errstr));
    if (ret != RD_KAFKA_CONF_OK)
      goto kafka_config_error;

    ret = rd_kafka_conf_set(rdkconf, "sasl.password", k->sasl.password, errstr,
                            sizeof(errstr));
    if (ret != RD_KAFKA_CONF_OK)
      goto kafka_config_error;
  }

  if (k->produce) {
    // rd_kafka_new() will take ownership and free the passed rd_kafka_conf_t object,
    // so we will need to create a copy first
    rd_kafka_conf_t *rdkconf_prod = rd_kafka_conf_dup(rdkconf);
    if (!rdkconf_prod)
      throw MemoryAllocationError();

    k->producer.client =
        rd_kafka_new(RD_KAFKA_PRODUCER, rdkconf_prod, errstr, sizeof(errstr));
    if (!k->producer.client)
      goto kafka_config_error;

    rd_kafka_topic_conf_t *topic_conf = rd_kafka_topic_conf_new();
    if (!topic_conf)
      throw MemoryAllocationError();

    ret = rd_kafka_topic_conf_set(topic_conf, "acks", "all", errstr,
                                  sizeof(errstr));
    if (ret != RD_KAFKA_CONF_OK)
      goto kafka_config_error;

    k->producer.topic =
        rd_kafka_topic_new(k->producer.client, k->produce, topic_conf);
    if (!k->producer.topic)
      throw MemoryAllocationError();

    n->logger->info("Connected producer to bootstrap server {}", k->server);
  }

  if (k->consume) {
    // rd_kafka_new() will take ownership and free the passed rd_kafka_conf_t object,
    // so we will need to create a copy first
    rd_kafka_conf_t *rdkconf_cons = rd_kafka_conf_dup(rdkconf);
    if (!rdkconf_cons)
      throw MemoryAllocationError();

    rd_kafka_topic_partition_list_t *partitions =
        rd_kafka_topic_partition_list_new(1);
    if (!partitions)
      throw MemoryAllocationError();

    rd_kafka_topic_partition_t *partition =
        rd_kafka_topic_partition_list_add(partitions, k->consume, 0);
    if (!partition)
      throw RuntimeError("Failed to add new partition");

    ret = rd_kafka_conf_set(rdkconf_cons, "group.id", k->consumer.group_id,
                            errstr, sizeof(errstr));
    if (ret != RD_KAFKA_CONF_OK)
      goto kafka_config_error;

    k->consumer.client =
        rd_kafka_new(RD_KAFKA_CONSUMER, rdkconf_cons, errstr, sizeof(errstr));
    if (!k->consumer.client)
      throw MemoryAllocationError();

    ret = rd_kafka_subscribe(k->consumer.client, partitions);
    if (ret != RD_KAFKA_RESP_ERR_NO_ERROR)
      throw RuntimeError("Error subscribing to {} at {}: {}", k->consume,
                         k->server, rd_kafka_err2str((rd_kafka_resp_err_t)ret));

    n->logger->info("Subscribed consumer from bootstrap server {}", k->server);
  }

  // Add client to global list of kafka clients
  // so that thread can call kafka loop for this client
  list_push(&clients, n);

  rd_kafka_conf_destroy(rdkconf);

  return 0;

kafka_config_error:
  rd_kafka_conf_destroy(rdkconf);

  throw RuntimeError(errstr);

  return -1;
}

int villas::node::kafka_stop(NodeCompat *n) {
  int ret;
  auto *k = n->getData<struct kafka>();

  if (k->producer.client) {
    ret = rd_kafka_flush(k->producer.client, k->timeout * 1000);
    if (ret != RD_KAFKA_RESP_ERR_NO_ERROR)
      n->logger->error("Failed to flush messages: {}",
                       rd_kafka_err2str((rd_kafka_resp_err_t)ret));

    /* If the output queue is still not empty there is an issue
		 * with producing messages to the clusters. */
    if (rd_kafka_outq_len(k->producer.client) > 0)
      n->logger->warn("{} message(s) were not delivered",
                      rd_kafka_outq_len(k->producer.client));
  }

  // Unregister client from global kafka client list
  // so that kafka loop is no longer invoked for this client
  // important to do that before disconnecting from broker, otherwise, kafka thread will attempt to reconnect
  list_remove_all(&clients, n);

  ret = queue_signalled_close(&k->queue);
  if (ret)
    return ret;

  return 0;
}

int villas::node::kafka_type_start(villas::node::SuperNode *sn) {
  int ret;

  logger = logging.get("node:kafka");

  ret = list_init(&clients);
  if (ret)
    goto kafka_error;

  // Start thread here to run kafka loop for registered clients
  ret = pthread_create(&thread, nullptr, kafka_loop_thread, nullptr);
  if (ret)
    goto kafka_error;

  return 0;

kafka_error:
  logger->warn("Error initialazing node type kafka");

  return ret;
}

int villas::node::kafka_type_stop() {
  int ret;

  // Stop thread here that executes kafka loop
  ret = pthread_cancel(thread);
  if (ret)
    return ret;

  logger->debug(
      "Called pthread_cancel() on kafka communication management thread.");

  ret = pthread_join(thread, nullptr);
  if (ret)
    goto kafka_error;

  // When this is called the list of clients should be empty
  if (list_length(&clients) > 0)
    throw RuntimeError(
        "List of kafka clients contains elements at time of destruction. Call "
        "node_stop for each kafka node before stopping node type!");

  ret = list_destroy(&clients, nullptr, false);
  if (ret)
    goto kafka_error;

  return 0;

kafka_error:
  logger->warn("Error stoping node type kafka");

  return ret;
}

int villas::node::kafka_read(NodeCompat *n, struct Sample *const smps[],
                             unsigned cnt) {
  int pulled;
  auto *k = n->getData<struct kafka>();
  struct Sample *smpt[cnt];

  pulled = queue_signalled_pull_many(&k->queue, (void **)smpt, cnt);

  sample_copy_many(smps, smpt, pulled);
  sample_decref_many(smpt, pulled);

  return pulled;
}

int villas::node::kafka_write(NodeCompat *n, struct Sample *const smps[],
                              unsigned cnt) {
  int ret;
  auto *k = n->getData<struct kafka>();

  size_t wbytes;

  char data[DEFAULT_FORMAT_BUFFER_LENGTH];

  ret = k->formatter->sprint(data, sizeof(data), &wbytes, smps, cnt);
  if (ret < 0)
    return ret;

  if (k->produce) {
    ret = rd_kafka_produce(k->producer.topic, RD_KAFKA_PARTITION_UA,
                           RD_KAFKA_MSG_F_COPY, data, wbytes, NULL, 0, NULL);

    if (ret != RD_KAFKA_RESP_ERR_NO_ERROR) {
      n->logger->warn("Publish failed");
      return -abs(ret);
    }
  } else
    n->logger->warn(
        "No produce possible because no produce topic is configured");

  return cnt;
}

int villas::node::kafka_poll_fds(NodeCompat *n, int fds[]) {
  auto *k = n->getData<struct kafka>();

  fds[0] = queue_signalled_fd(&k->queue);

  return 1;
}

static NodeCompatType p;

__attribute__((constructor(110))) static void register_plugin() {
  p.name = "kafka";
  p.description = "Kafka event message streaming (rdkafka)";
  p.vectorize = 0;
  p.size = sizeof(struct kafka);
  p.type.start = kafka_type_start;
  p.type.stop = kafka_type_stop;
  p.destroy = kafka_destroy;
  p.prepare = kafka_prepare;
  p.parse = kafka_parse;
  p.prepare = kafka_prepare;
  p.print = kafka_print;
  p.init = kafka_init;
  p.destroy = kafka_destroy;
  p.start = kafka_start;
  p.stop = kafka_stop;
  p.read = kafka_read;
  p.write = kafka_write;
  p.reverse = kafka_reverse;
  p.poll_fds = kafka_poll_fds;

  static NodeCompatFactory ncp(&p);
}
