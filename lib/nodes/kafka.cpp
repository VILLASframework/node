/** Node type: kafka
 *
 * @author Juan Pablo Noreña <jpnorenam@unal.edu.co>
 * @copyright 2021, Universidad Nacional de Colombia
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <cstring>
#include <librdkafka/rdkafkacpp.h>

#include <villas/nodes/kafka.hpp>
#include <villas/plugin.h>
#include <villas/utils.hpp>
#include <villas/format_type.h>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::utils;

// Each process has a list of clients for which a thread invokes the kafka loop
static struct vlist clients;
static pthread_t thread;
static Logger logger;

static void kafka_message(void *ctx, const rd_kafka_message_t *msg)
{
	int ret;
	struct vnode *n = (struct vnode *) ctx;
	struct kafka *k = (struct kafka *) n->_vd;
	struct sample *smps[n->in.vectorize];

	n->logger->debug("Received a message of {} bytes from broker {}", msg->len, k->server);

	ret = sample_alloc_many(&k->pool, smps, n->in.vectorize);
	if (ret <= 0) {
		n->logger->warn("Pool underrun in consumer");
		return;
	}

	ret = io_sscan(&k->io, (char *) msg->payload, msg->len, nullptr, smps, n->in.vectorize);
	if (ret < 0) {
		n->logger->warn("Received an invalid message");
		n->logger->warn("  Payload: {}", (char *) msg->payload);
		return;
	}

	if (ret == 0) {
		n->logger->debug("Skip empty message");
		sample_decref_many(smps, n->in.vectorize);
		return;
	}

	ret = queue_signalled_push_many(&k->queue, (void **) smps, n->in.vectorize);
	if (ret < (int) n->in.vectorize)
		n->logger->warn("Failed to enqueue samples");
}

static void * kafka_loop_thread(void *ctx)
{
	int ret;

	// Set the cancel type of this thread to async
	ret = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, nullptr);
	if (ret != 0)
		throw RuntimeError("Unable to set cancel type of Kafka communication thread to asynchronous.");

	while (true) {
		for (unsigned i = 0; i < vlist_length(&clients); i++) {
			struct vnode *n = (struct vnode *) vlist_at(&clients, i);
			struct kafka *k = (struct kafka *) n->_vd;

			// Execute kafka loop for this client
			if (k->consumer.client) {
				rd_kafka_message_t *msg = rd_kafka_consumer_poll(k->consumer.client, k->timeout);

				if (msg) {
					kafka_message((void *) n, msg);
					rd_kafka_message_destroy(msg);
				}
			}
		}
	}

	return nullptr;
}

int kafka_reverse(struct vnode *n)
{
	struct kafka *k = (struct kafka *) n->_vd;

	SWAP(k->produce, k->consume);

	return 0;
}

int kafka_init(struct vnode *n)
{
	int ret;
	struct kafka *k = (struct kafka *) n->_vd;

	/* Default values */
	k->server = nullptr;
	k->protocol = nullptr;
	k->produce = nullptr;
	k->consume = nullptr;
	k->client_id = nullptr;

	k->consumer.client = nullptr;
	k->consumer.group_id = nullptr;
	k->producer.client = nullptr;
	k->producer.topic = nullptr;

	k->sasl.mechanism = nullptr;
	k->sasl.username = nullptr;
	k->sasl.password = nullptr;

	k->ssl.ca = nullptr;

	ret = 0;

	return ret;
}

int kafka_parse(struct vnode *n, json_t *json)
{
	int ret;
	struct kafka *k = (struct kafka *) n->_vd;

	const char *server;
	const char *format = "villas.binary";
	const char *produce = nullptr;
	const char *consume = nullptr;
	const char *protocol = nullptr;
	const char *client_id = nullptr;
	const char *group_id = nullptr;

	json_error_t err;
	json_t *json_ssl = nullptr;
	json_t *json_sasl = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s?: { s?: s }, s?: { s?: s, s?: s }, s?: s, s: s, s?: i, s?: s, s?: s, s?: o, s?: o }",
		"out",
			"produce", &produce,
		"in",
			"consume", &consume,
			"group_id", &group_id,
		"format", &format,
		"server", &server,
		"timeout", &k->timeout,
		"protocol", &protocol,
		"client_id", &client_id,
		"ssl", &json_ssl,
		"sasl", &json_sasl
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-kafka");

	k->server = strdup(server);
	k->produce = produce ? strdup(produce) : nullptr;
	k->consume = consume ? strdup(consume) : nullptr;
	k->protocol = protocol ? strdup(protocol) : nullptr;
	k->client_id = client_id ? strdup(client_id) : nullptr;
	k->consumer.group_id = group_id ? strdup(group_id) : nullptr;

	if (!k->produce && !k->consume)
		throw ConfigError(json, "node-config-node-kafka", "At least one topic has to be specified for node {}", node_name(n));

	if (json_ssl) {

		const char *ca = nullptr;

		ret = json_unpack_ex(json_ssl, &err, 0, "{ s?: s }",
			"ca", &ca
		);
		if (ret)
			throw ConfigError(json_ssl, err, "node-config-node-kafka-ssl", "Failed to parse SSL configuration of node {}", node_name(n));

		if (!ca)
			throw ConfigError(json_ssl, "node-config-node-kafka-ssl", "'ca' settings must be set for node {}.", node_name(n));

		k->ssl.ca = ca ? strdup(ca) : nullptr;
	}

	if (json_sasl) {
		const char *mechanism = nullptr;
		const char *username = nullptr;
		const char *password = nullptr;

		ret = json_unpack_ex(json_ssl, &err, 0, "{ s?: s, s?: s, s?: s }",
			"mechanism", &mechanism,
			"username", &username,
			"password", &password
		);
		if (ret)
			throw ConfigError(json_sasl, err, "node-config-node-kafka-sasl", "Failed to parse SASL configuration of node {}", node_name(n));

		if (!username && !password && !mechanism)
			throw ConfigError(json_sasl, "node-config-node-kafka-sasl", "Either 'sasl.mechanism', 'sasl.username' or 'sasl.password' settings must be set for node {}.", node_name(n));

		k->sasl.mechanism = mechanism ? strdup(mechanism) : nullptr;
		k->sasl.username = username ? strdup(username) : nullptr;
		k->sasl.password = password ? strdup(password) : nullptr;
	}

	k->format = format_type_lookup(format);
	if (!k->format)
		throw ConfigError(json_ssl, "node-config-node-kafka-format", "Invalid format '{}' for node {}", format, node_name(n));

	return 0;
}

int kafka_prepare(struct vnode *n)
{
	int ret;
	struct kafka *k = (struct kafka *) n->_vd;

	ret = io_init(&k->io, k->format, &n->in.signals, (int) SampleFlags::HAS_ALL & ~(int) SampleFlags::HAS_OFFSET);
	if (ret)
		return ret;

	ret = pool_init(&k->pool, 1024, SAMPLE_LENGTH(vlist_length(&n->in.signals)));
	if (ret)
		return ret;

	ret = queue_signalled_init(&k->queue, 1024);
	if (ret)
		return ret;

	return 0;
}

char * kafka_print(struct vnode *n)
{
	struct kafka *k = (struct kafka *) n->_vd;

	char *buf = nullptr;

	strcatf(&buf, "format=%s, bootstrap.server=%s, client.id=%s, security.protocol=%s", format_type_name(k->format),
		k->server,
		k->client_id,
		k->protocol
	);

	/* Only show if not default */
	if (k->produce)
		strcatf(&buf, ", out.produce=%s", k->produce);

	if (k->consume)
		strcatf(&buf, ", in.consume=%s", k->consume);

	return buf;
}

int kafka_destroy(struct vnode *n)
{
	int ret;
	struct kafka *k = (struct kafka *) n->_vd;

	if (k->producer.client)
		rd_kafka_destroy(k->producer.client);

	if (k->consumer.client)
		rd_kafka_destroy(k->consumer.client);

	ret = io_destroy(&k->io);
	if (ret)
		return ret;

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

int kafka_start(struct vnode *n)
{
	int ret;
	struct kafka *k = (struct kafka *) n->_vd;

	char *errstr;
	rd_kafka_conf_t *rdkconf = rd_kafka_conf_new();

	if (rd_kafka_conf_set(rdkconf, "client.id", k->client_id, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
		ret = 1;

	if (rd_kafka_conf_set(rdkconf, "bootstrap.servers", k->server, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
		ret = 1;

	if (rd_kafka_conf_set(rdkconf, "security.protocol", k->protocol, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
		ret = 1;

	if (!strcmp(k->protocol, "SASL_SSL") || !strcmp(k->protocol, "SSL")) {
		if (rd_kafka_conf_set(rdkconf, "ssl.ca.location", k->ssl.ca, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
			ret = 1;
	}

	if (!strcmp(k->protocol, "SASL_PLAINTEXT") || !strcmp(k->protocol, "SASL_SSL")) {
		if (rd_kafka_conf_set(rdkconf, "sasl.mechanisms", k->sasl.mechanism, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
			ret = 1;
		if (rd_kafka_conf_set(rdkconf, "sasl.username", k->sasl.username, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
			ret = 1;
		if (rd_kafka_conf_set(rdkconf, "sasl.password", k->sasl.password, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
			ret = 1;
	}

	if (ret)
		goto kafka_config_error;

	if (k->produce) {
		k->producer.client = rd_kafka_new(RD_KAFKA_PRODUCER, rdkconf, errstr, sizeof(errstr));
		rd_kafka_topic_conf_t *topic_conf = rd_kafka_topic_conf_new();
		if (rd_kafka_topic_conf_set(topic_conf, "acks", "all", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
			ret = 1;
		k->producer.topic = rd_kafka_topic_new(k->producer.client, k->produce, topic_conf);
		n->logger->info("Connected producer to bootstrap server {}", k->server);
	}

	// if (k->consume) { # rd_kafka_new() method not working for two simultaneously clients (SIGSEGV)
	else if (k->consume) {
		rd_kafka_topic_partition_list_t *partitions = rd_kafka_topic_partition_list_new(0);
		rd_kafka_topic_partition_list_add(partitions, k->consume, 0);

		if (rd_kafka_conf_set(rdkconf, "group.id", k->consumer.group_id, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
			ret = 1;

		k->consumer.client = rd_kafka_new(RD_KAFKA_CONSUMER, rdkconf, errstr, sizeof(errstr));
		rd_kafka_subscribe(k->consumer.client, partitions);

		n->logger->info("Subscribed consumer from bootstrap server {}", k->server);
	}

	if (!k->consumer.client && !k->producer.client)
		goto kafka_server_error;

	// Add client to global list of kafka clients
	// so that thread can call kafka loop for this client
	vlist_push(&clients, n);

	return 0;

kafka_config_error:
	rd_kafka_conf_destroy(rdkconf);
	n->logger->warn("{}", errstr);
	return ret;

kafka_server_error:
	n->logger->warn("Error subscribing to {} at {} ", k->consume, k->server);
	return ret;

}

int kafka_stop(struct vnode *n)
{
	// Unregister client from global kafka client list
	// so that kafka loop is no longer invoked for this client
	// important to do that before disconnecting from broker, otherwise, kafka thread will attempt to reconnect
	vlist_remove_all(&clients, n);

#if 0
	int ret;
	struct kafka *k = (struct kafka *) n->_vd;

	ret = kafka_disconnect(k->client);
	if (ret != RD_KAFKA_ERR_NO_ERROR)
		goto kafka_error;

kafka_error:
	n->logger->warn("{}", kafka_strerror(ret));

	return ret;
#else
	return 0;
#endif
}

int kafka_type_start(villas::node::SuperNode *sn)
{
	int ret;

	logger = logging.get("node:kafka");

	ret = vlist_init(&clients);
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

int kafka_type_stop()
{
	int ret;

	// Stop thread here that executes kafka loop
	ret = pthread_cancel(thread);
	if (ret)
		return ret;

	logger->debug("Called pthread_cancel() on kafka communication management thread.");

	ret = pthread_join(thread, nullptr);
	if (ret)
		goto kafka_error;

	// When this is called the list of clients should be empty
	if (vlist_length(&clients) > 0)
		throw RuntimeError("List of kafka clients contains elements at time of destruction. Call node_stop for each kafka node before stopping node type!");

	ret = vlist_destroy(&clients, nullptr, false);
	if (ret)
		goto kafka_error;

	return 0;

kafka_error:
	logger->warn("Error stoping node type kafka");

	return ret;
}

int kafka_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int pulled;
	struct kafka *k = (struct kafka *) n->_vd;
	struct sample *smpt[cnt];

	pulled = queue_signalled_pull_many(&k->queue, (void **) smpt, cnt);

	sample_copy_many(smps, smpt, pulled);
	sample_decref_many(smpt, pulled);

	return pulled;
}

int kafka_write(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct kafka *k = (struct kafka *) n->_vd;

	size_t wbytes;

	char data[1500];

	ret = io_sprint(&k->io, data, sizeof(data), &wbytes, smps, cnt);
	if (ret < 0)
		return ret;

	if (k->produce) {
		ret = rd_kafka_produce(k->producer.topic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY,
			data, wbytes, NULL, 0, NULL);

		if (ret != RD_KAFKA_RESP_ERR_NO_ERROR) {
			n->logger->warn("Publish failed");
			return -abs(ret);
		}
	}
	else
		n->logger->warn("No produce possible because no produce topic is configured");

	return cnt;
}

int kafka_poll_fds(struct vnode *n, int fds[])
{
	struct kafka *k = (struct kafka *) n->_vd;

	fds[0] = queue_signalled_fd(&k->queue);

	return 1;
}

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "kafka";
	p.description		= "Kafka event message streaming (rdkafka)";
	p.type			= PluginType::NODE;
	p.node.instances.state = State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct kafka);
	p.node.type.start	= kafka_type_start;
	p.node.type.stop	= kafka_type_stop;
	p.node.destroy		= kafka_destroy;
	p.node.prepare		= kafka_prepare;
	p.node.parse		= kafka_parse;
	p.node.prepare		= kafka_prepare;
	p.node.print		= kafka_print;
	p.node.init		= kafka_init;
	p.node.destroy		= kafka_destroy;
	p.node.start		= kafka_start;
	p.node.stop		= kafka_stop;
	p.node.read		= kafka_read;
	p.node.write		= kafka_write;
	p.node.reverse		= kafka_reverse;
	p.node.poll_fds		= kafka_poll_fds;

	int ret = vlist_init(&p.node.instances);
	if (!ret)
		vlist_init_and_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
