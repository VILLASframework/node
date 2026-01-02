/* Node type: kafka.
 *
 * Author: Juan Pablo Noreña <jpnorenam@unal.edu.co>
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 * SPDX-FileCopyrightText: 2021 Universidad Nacional de Colombia
 * SPDX-FileCopyrightText: 2025 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>

#include <librdkafka/rdkafkacpp.h>
#include <sys/eventfd.h>

#include <villas/exceptions.hpp>
#include <villas/format.hpp>
#include <villas/log.hpp>
#include <villas/node_compat.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::utils;
using namespace villas::node;

static Logger logger;

class KafkaNode : public Node, public RdKafka::EventCb {

protected:
  // Settings.
  std::chrono::milliseconds timeout;
  std::string server;    // Hostname/IP:Port address of the bootstrap server.
  std::string protocol;  // Security protocol.
  std::string produce;   // Producer topic.
  std::string consume;   // Consumer topic.
  std::string client_id; // Client ID.
  std::string group_id;  // Group ID.
  std::string ssl_ca;    // SSL CA file.

  struct {
    std::unique_ptr<RdKafka::Producer> client;
    std::unique_ptr<RdKafka::Topic> topic;
  } producer;

  struct {
    std::unique_ptr<RdKafka::Consumer> client;
    std::unique_ptr<RdKafka::Topic> topic;
    std::unique_ptr<RdKafka::Queue> queue;
    int eventFd;
  } consumer;

  struct {
    std::string mechanisms; // SASL mechanisms.
    std::string username;   // SSL CA path.
    std::string password;   // SSL certificate.
  } sasl;

  std::unique_ptr<Format> formatter;

  int _read(struct Sample *smps[], unsigned cnt) override {
    assert(consumer.client != nullptr);

    auto msg = consumer.client->consume(consumer.queue.get(), timeout.count());

    auto ret = formatter->sscan((char *)msg->payload(), msg->len(), nullptr,
                                smps, cnt);
    if (ret < 0) {
      logger->warn("Received an invalid message");
      logger->warn("  Payload: {}", (char *)msg->payload());
      return -1;
    }

    return ret;
  }

  int _write(struct Sample *smps[], unsigned cnt) override {
    assert(producer.client != nullptr);

    size_t wbytes;

    char data[DEFAULT_FORMAT_BUFFER_LENGTH];

    auto ret = formatter->sprint(data, sizeof(data), &wbytes, smps, cnt);
    if (ret < 0)
      return ret;

    if (!produce.empty()) {
      auto ret = producer.client->produce(
          producer.topic.get(), RdKafka::Topic::PARTITION_UA,
          RdKafka::Producer::RK_MSG_COPY, data, wbytes, NULL, 0, NULL);
      if (ret != RdKafka::ErrorCode::ERR_NO_ERROR) {
        logger->warn("Publish failed");
        return -abs(ret);
      }
    } else
      logger->warn(
          "No produce possible because no produce topic is configured");

    return cnt;
  }

  int startProducer() {
    std::string errstr;

    auto conf_prod = createCommonConf();
    if (!conf_prod)
      throw MemoryAllocationError();

    producer.client = std::unique_ptr<RdKafka::Producer>(
        RdKafka::Producer::create(conf_prod.get(), errstr));
    if (!producer.client)
      throw RuntimeError("{}", errstr);

    auto topic_conf = std::unique_ptr<RdKafka::Conf>(
        RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC));
    if (!topic_conf)
      throw MemoryAllocationError();

    auto cr = topic_conf->set("acks", "all", errstr);
    if (cr != RdKafka::Conf::CONF_OK)
      throw RuntimeError("{}", errstr);

    producer.topic = std::unique_ptr<RdKafka::Topic>(RdKafka::Topic::create(
        producer.client.get(), produce, topic_conf.get(), errstr));
    if (!producer.topic)
      throw MemoryAllocationError();

    logger->info("Connected producer to bootstrap server {}", server);

    return 0;
  }

  int startConsumer() {
    std::string errstr;

    auto conf_cons = createCommonConf();
    if (!conf_cons)
      throw MemoryAllocationError();

    auto cr = conf_cons->set("group.id", group_id, errstr);
    if (cr != RdKafka::Conf::CONF_OK)
      throw RuntimeError("{}", errstr);

    consumer.client = std::unique_ptr<RdKafka::Consumer>(
        RdKafka::Consumer::create(conf_cons.get(), errstr));
    if (!consumer.client)
      throw MemoryAllocationError();

    consumer.topic = std::unique_ptr<RdKafka::Topic>(RdKafka::Topic::create(
        consumer.client.get(), consume, nullptr, errstr));
    if (!consumer.topic)
      throw MemoryAllocationError();

    consumer.queue = std::unique_ptr<RdKafka::Queue>(
        RdKafka::Queue::create(consumer.client.get()));
    if (!consumer.queue)
      throw MemoryAllocationError();

    consumer.eventFd = eventfd(0, 0);

    uint64_t incr = 1;
    consumer.queue->io_event_enable(consumer.eventFd, &incr, sizeof(incr));

    auto ec = consumer.client->start(consumer.topic.get(), 0, 0,
                                     consumer.queue.get());
    if (ec != RdKafka::ErrorCode::ERR_NO_ERROR)
      throw RuntimeError("Error subscribing to {} at {}: {}", consume, server,
                         RdKafka::err2str(ec));

    logger->info("Subscribed consumer from bootstrap server {}", server);

    return 0;
  }

  std::unique_ptr<RdKafka::Conf> createCommonConf() {
    std::string errstr;

    auto conf = std::unique_ptr<RdKafka::Conf>(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (!conf)
      throw MemoryAllocationError();

    auto ret = conf->set("event_cb", this, errstr);
    if (ret != RdKafka::Conf::CONF_OK)
      throw RuntimeError("{}", errstr);

    ret = conf->set("client.id", client_id, errstr);
    if (ret != RdKafka::Conf::CONF_OK)
      throw RuntimeError("{}", errstr);

    ret = conf->set("bootstrap.servers", server, errstr);
    if (ret != RdKafka::Conf::CONF_OK)
      throw RuntimeError("{}", errstr);

    ret = conf->set("security.protocol", protocol, errstr);
    if (ret != RdKafka::Conf::CONF_OK)
      throw RuntimeError("{}", errstr);

    if (protocol == "SASL_SSL" || protocol == "SSL") {
      ret = conf->set("ssl.ca.location", ssl_ca, errstr);
      if (ret != RdKafka::Conf::CONF_OK)
        throw RuntimeError("{}", errstr);
    }

    if (protocol == "SASL_PLAINTEXT" || protocol == "SASL_SSL") {
      ret = conf->set("sasl.mechanisms", sasl.mechanisms, errstr);
      if (ret != RdKafka::Conf::CONF_OK)
        throw RuntimeError("{}", errstr);

      ret = conf->set("sasl.username", sasl.username, errstr);
      if (ret != RdKafka::Conf::CONF_OK)
        throw RuntimeError("{}", errstr);

      ret = conf->set("sasl.password", sasl.password, errstr);
      if (ret != RdKafka::Conf::CONF_OK)
        throw RuntimeError("{}", errstr);
    }

    return conf;
  }

public:
  KafkaNode(const uuid_t &id = {}, const std::string &name = "")
      : Node(id, name), timeout(1000), client_id("villas-node"), producer({}),
        consumer({.eventFd = -1}) {}

  virtual ~KafkaNode() {}

  int prepare() override {
    formatter->start(getInputSignals(false), ~(int)SampleFlags::HAS_OFFSET);

    return Node::prepare();
  }

  int parse(json_t *json) override {
    int ret = Node::parseCommon(json);
    if (ret)
      return ret;

    const char *svr;
    const char *prod = nullptr;
    const char *cons = nullptr;
    const char *proto;
    const char *cid = nullptr;
    const char *gid = nullptr;
    double to = -1;

    json_t *json_ssl = nullptr;
    json_t *json_sasl = nullptr;
    json_t *json_format = nullptr;

    json_error_t err;
    ret = json_unpack_ex(json, &err, 0,
                         "{ s?: { s?: s }, s?: { s?: s, s?: s }, s?: o, s: s, "
                         "s?: F, s: s, s?: s, s?: o, s?: o }",
                         "out", "topic", &prod, "in", "topic", &cons,
                         "group_id", &gid, "format", &json_format, "server",
                         &svr, "timeout", &to, "protocol", &proto, "client_id",
                         &cid, "ssl", &json_ssl, "sasl", &json_sasl);
    if (ret)
      throw ConfigError(json, err, "node-config-node-kafka");

    server = svr;
    protocol = proto;

    if (prod)
      produce = prod;

    if (cons)
      consume = cons;

    if (cid)
      client_id = cid;

    if (gid)
      group_id = gid;

    if (to >= 0)
      timeout = std::chrono::milliseconds((int)(to * 1000));

    if (protocol != "SSL" && protocol != "PLAINTEXT" &&
        protocol != "SASL_SSL" && protocol != "SASL_PLAINTEXT")
      throw ConfigError(json, "node-config-node-kafka-protocol",
                        "Invalid security protocol: {}", protocol);

    if (produce.empty() && consume.empty())
      throw ConfigError(json, "node-config-node-kafka",
                        "At least one topic has to be specified for node {}",
                        getName());

    if (json_ssl) {
      const char *ca = nullptr;

      ret = json_unpack_ex(json_ssl, &err, 0, "{ s?: s }", "ca", &ca);
      if (ret)
        throw ConfigError(json_ssl, err, "node-config-node-kafka-ssl",
                          "Failed to parse SSL configuration of node {}",
                          getName());

      if (ca)
        ssl_ca = ca;
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

      sasl.mechanisms = mechanisms;
      sasl.username = username;
      sasl.password = password;
    }

    // Format
    formatter = std::unique_ptr<Format>(
        json_format ? FormatFactory::make(json_format)
                    : FormatFactory::make("villas.binary"));
    if (!formatter)
      throw ConfigError(json_format, "node-config-node-kafka-format",
                        "Invalid format configuration");

    return 0;
  }

  const std::string &getDetails() override {
    details = fmt::format("server={}, client_id={}, protocol={}", server,
                          client_id, protocol);

    if (!produce.empty())
      details += fmt::format(", produce={}", produce);

    if (!consume.empty())
      details += fmt::format(", consume={}", consume);

    return details;
  }

  int start() override {
    if (!produce.empty()) {
      auto ret = startProducer();
      if (ret)
        return ret;
    }

    if (!consume.empty()) {
      auto ret = startConsumer();
      if (ret)
        return ret;
    }

    int ret = Node::start();
    if (!ret)
      state = State::STARTED;

    return 0;
  }

  int stop() override {
    int ret = Node::stop();
    if (ret)
      return ret;

    if (producer.client) {
      auto ret = producer.client->flush(timeout.count());
      if (ret != RdKafka::ErrorCode::ERR_NO_ERROR)
        logger->error("Failed to flush messages: {}", RdKafka::err2str(ret));

      // If the output queue is still not empty there is an issue
      // with producing messages to the clusters.
      if (producer.client->outq_len() > 0)
        logger->warn("{} message(s) were not delivered",
                     producer.client->outq_len());
    }

    return 0;
  }

  int reverse() override {
    SWAP(produce, consume);

    return 0;
  }

  std::vector<int> getPollFDs() override {
    if (consumer.eventFd >= 0)
      return {consumer.eventFd};

    return {};
  }

  void event_cb(RdKafka::Event &event) override {
    switch (event.type()) {
    case RdKafka::Event::EVENT_ERROR:
      logger->error("Kafka error: {}", event.str());
      break;

    case RdKafka::Event::EVENT_STATS:
      logger->info("Kafka stats: {}", event.str());
      break;

    case RdKafka::Event::EVENT_LOG:
      switch (event.severity()) {
      case RdKafka::Event::EVENT_SEVERITY_DEBUG:
        logger->debug("{}", event.str());
        break;

      case RdKafka::Event::EVENT_SEVERITY_NOTICE:
      case RdKafka::Event::EVENT_SEVERITY_INFO:
        logger->info("{}", event.str());
        break;

      case RdKafka::Event::EVENT_SEVERITY_ALERT:
      case RdKafka::Event::EVENT_SEVERITY_WARNING:
        logger->warn("{}", event.str());
        break;

      case RdKafka::Event::EVENT_SEVERITY_ERROR:
      case RdKafka::Event::EVENT_SEVERITY_CRITICAL:
      case RdKafka::Event::EVENT_SEVERITY_EMERG:
        logger->error("{}", event.str());
        break;
      }

      break;

    default:
      logger->info("Kafka event {}: {}", (int)event.type(), event.str());
      break;
    }
  }
};

// Register node
static char n[] = "kafka";
static char d[] = "Kafka event message streaming (rdkafka)";
static NodePlugin<KafkaNode, n, d,
                  (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_WRITE |
                      (int)NodeFactory::Flags::SUPPORTS_POLL>
    p;
