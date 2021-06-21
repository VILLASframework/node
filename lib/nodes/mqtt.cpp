/** Node type: mqtt
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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
#include <mosquitto.h>

#include <villas/node.h>
#include <villas/nodes/mqtt.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

// Each process has a list of clients for which a thread invokes the mosquitto loop
static struct vlist clients;
static pthread_t thread;
static Logger logger;

static void * mosquitto_loop_thread(void *ctx)
{
	int ret;

	// Set the cancel type of this thread to async
	ret = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, nullptr);
	if (ret != 0)
		throw RuntimeError("Unable to set cancel type of MQTT communication thread to asynchronous.");

	while (true) {
		for (unsigned i = 0; i < vlist_length(&clients); i++) {
			struct vnode *n = (struct vnode *) vlist_at(&clients, i);
			struct mqtt *m = (struct mqtt *) n->_vd;

			// Execute mosquitto loop for this client
			ret = mosquitto_loop(m->client, 0, 1);
			if (ret) {
				n->logger->warn("Connection error: {}, attempting reconnect", mosquitto_strerror(ret));

				ret = mosquitto_reconnect(m->client);
				if (ret != MOSQ_ERR_SUCCESS)
					n->logger->warn("Reconnection to broker failed: {}", mosquitto_strerror(ret));
				else
					n->logger->warn("Successfully reconnected to broker: {}", mosquitto_strerror(ret));

				ret = mosquitto_loop(m->client, 0, 1);
				if (ret != MOSQ_ERR_SUCCESS)
					n->logger->warn("Persisting connection error: {}", mosquitto_strerror(ret));
			}
		}
	}

	return nullptr;
}

static void mqtt_log_cb(struct mosquitto *mosq, void *ctx, int level, const char *str)
{
	struct vnode *n = (struct vnode *) ctx;

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

static void mqtt_connect_cb(struct mosquitto *mosq, void *ctx, int result)
{
	struct vnode *n = (struct vnode *) ctx;
	struct mqtt *m = (struct mqtt *) n->_vd;

	int ret;

	n->logger->info("Connected to broker {}", m->host);

	if (m->subscribe) {
		ret = mosquitto_subscribe(m->client, nullptr, m->subscribe, m->qos);
		if (ret)
			n->logger->warn("Failed to subscribe to topic '{}': {}", m->subscribe, mosquitto_strerror(ret));
	}
	else
		n->logger->warn("No subscription as no topic is configured");
}

static void mqtt_disconnect_cb(struct mosquitto *mosq, void *ctx, int result)
{
	struct vnode *n = (struct vnode *) ctx;
	struct mqtt *m = (struct mqtt *) n->_vd;

	n->logger->info("Disconnected from broker {}", m->host);
}

static void mqtt_message_cb(struct mosquitto *mosq, void *ctx, const struct mosquitto_message *msg)
{
	int ret;
	struct vnode *n = (struct vnode *) ctx;
	struct mqtt *m = (struct mqtt *) n->_vd;
	struct sample *smps[n->in.vectorize];

	n->logger->debug("Received a message of {} bytes from broker {}", msg->payloadlen, m->host);

	ret = sample_alloc_many(&m->pool, smps, n->in.vectorize);
	if (ret <= 0) {
		n->logger->warn("Pool underrun in subscriber");
		return;
	}

	ret = m->formatter->sscan((char *) msg->payload, msg->payloadlen, nullptr, smps, n->in.vectorize);
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

	ret = queue_signalled_push_many(&m->queue, (void **) smps, n->in.vectorize);
	if (ret < (int) n->in.vectorize)
		n->logger->warn("Failed to enqueue samples");
}

static void mqtt_subscribe_cb(struct mosquitto *mosq, void *ctx, int mid, int qos_count, const int *granted_qos)
{
	struct vnode *n = (struct vnode *) ctx;
	struct mqtt *m = (struct mqtt *) n->_vd;

	n->logger->info("Subscribed to broker {}", m->host);
}

int mqtt_reverse(struct vnode *n)
{
	struct mqtt *m = (struct mqtt *) n->_vd;

	SWAP(m->publish, m->subscribe);

	return 0;
}

int mqtt_init(struct vnode *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	m->client = mosquitto_new(nullptr, true, (void *) n);
	if (!m->client)
		return -1;

	ret = mosquitto_threaded_set(m->client, true);
	if (ret)
		goto mosquitto_error;

	mosquitto_log_callback_set(m->client, mqtt_log_cb);
	mosquitto_connect_callback_set(m->client, mqtt_connect_cb);
	mosquitto_disconnect_callback_set(m->client, mqtt_disconnect_cb);
	mosquitto_message_callback_set(m->client, mqtt_message_cb);
	mosquitto_subscribe_callback_set(m->client, mqtt_subscribe_cb);

	/* Default values */
	m->port = 1883;
	m->qos = 0;
	m->retain = 0;
	m->keepalive = 5; /* 5 second, minimum required for libmosquitto */

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

	return 0;

mosquitto_error:
	n->logger->warn("{}", mosquitto_strerror(ret));

	return ret;
}

int mqtt_parse(struct vnode *n, json_t *json)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	const char *host;
	const char *publish = nullptr;
	const char *subscribe = nullptr;
	const char *username = nullptr;
	const char *password = nullptr;

	json_error_t err;
	json_t *json_ssl = nullptr;
	json_t *json_format = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s?: { s?: s }, s?: { s?: s }, s?: o, s: s, s?: i, s?: i, s?: i, s?: b, s?: s, s?: s, s?: o }",
		"out",
			"publish", &publish,
		"in",
			"subscribe", &subscribe,
		"format", &json_format,
		"host", &host,
		"port", &m->port,
		"qos", &m->qos,
		"keepalive", &m->keepalive,
		"retain", &m->retain,
		"username", &username,
		"password", &password,
		"ssl", &json_ssl
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-mqtt");

	m->host = strdup(host);
	m->publish = publish ? strdup(publish) : nullptr;
	m->subscribe = subscribe ? strdup(subscribe) : nullptr;
	m->username = username ? strdup(username) : nullptr;
	m->password = password ? strdup(password) : nullptr;

	if (!m->publish && !m->subscribe)
		throw ConfigError(json, "node-config-node-mqtt", "At least one topic has to be specified for node {}", node_name(n));

	if (json_ssl) {
		m->ssl.enabled = 1;

		const char *cafile = nullptr;
		const char *capath = nullptr;
		const char *certfile = nullptr;
		const char *keyfile = nullptr;

		ret = json_unpack_ex(json_ssl, &err, 0, "{ s?: b, s?: b, s?: s, s?: s, s?: s, s?: s }",
			"enabled", &m->ssl.enabled,
			"insecure", &m->ssl.insecure,
			"cafile", &cafile,
			"capath", &capath,
			"certfile", &certfile,
			"keyfile", &keyfile
		);
		if (ret)
			throw ConfigError(json_ssl, err, "node-config-node-mqtt-ssl", "Failed to parse SSL configuration of node {}", node_name(n));

		if (m->ssl.enabled && !cafile && !capath)
			throw ConfigError(json_ssl, "node-config-node-mqtt-ssl", "Either 'ssl.cafile' or 'ssl.capath' settings must be set for node {}.", node_name(n));

		m->ssl.cafile = cafile ? strdup(cafile) : nullptr;
		m->ssl.capath = capath ? strdup(capath) : nullptr;
		m->ssl.certfile = certfile ? strdup(certfile) : nullptr;
		m->ssl.keyfile = keyfile ? strdup(keyfile) : nullptr;
	}

	/* Format */
	m->formatter = json_format
			? FormatFactory::make(json_format)
			: FormatFactory::make("json");
	if (!m->formatter)
		throw ConfigError(json_format, "node-config-node-mqtt-format", "Invalid format configuration");
	return 0;
}

int mqtt_check(struct vnode *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	ret = mosquitto_sub_topic_check(m->subscribe);
	if (ret != MOSQ_ERR_SUCCESS)
		throw RuntimeError("Invalid subscribe topic: '{}': {}", m->subscribe, mosquitto_strerror(ret));

	ret = mosquitto_pub_topic_check(m->publish);
	if (ret != MOSQ_ERR_SUCCESS)
		throw RuntimeError("Invalid publish topic: '{}': {}", m->publish, mosquitto_strerror(ret));

	return 0;
}

int mqtt_prepare(struct vnode *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	m->formatter->start(&n->in.signals, ~(int) SampleFlags::HAS_OFFSET);

	ret = pool_init(&m->pool, 1024, SAMPLE_LENGTH(vlist_length(&n->in.signals)));
	if (ret)
		return ret;

	ret = queue_signalled_init(&m->queue, 1024);
	if (ret)
		return ret;

	return 0;
}

char * mqtt_print(struct vnode *n)
{
	struct mqtt *m = (struct mqtt *) n->_vd;

	char *buf = nullptr;

	strcatf(&buf, "host=%s, port=%d, keepalive=%d, ssl=%s",
		m->host,
		m->port,
		m->keepalive,
		m->ssl.enabled ? "yes" : "no"
	);

	/* Only show if not default */
	if (m->username)
		strcatf(&buf, ", username=%s", m->username);

	if (m->publish)
		strcatf(&buf, ", out.publish=%s", m->publish);

	if (m->subscribe)
		strcatf(&buf, ", in.subscribe=%s", m->subscribe);

	return buf;
}

int mqtt_destroy(struct vnode *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	mosquitto_destroy(m->client);

	delete m->formatter;

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

	free(m->host);

	return 0;
}

int mqtt_start(struct vnode *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	if (m->username && m->password) {
		ret = mosquitto_username_pw_set(m->client, m->username, m->password);
		if (ret != MOSQ_ERR_SUCCESS)
			goto mosquitto_error;
	}

	if (m->ssl.enabled) {
		ret = mosquitto_tls_set(m->client, m->ssl.cafile, m->ssl.capath, m->ssl.certfile, m->ssl.keyfile, nullptr);
		if (ret != MOSQ_ERR_SUCCESS)
			goto mosquitto_error;

		ret = mosquitto_tls_insecure_set(m->client, m->ssl.insecure);
		if (ret != MOSQ_ERR_SUCCESS)
			goto mosquitto_error;
	}

	ret = mosquitto_connect(m->client, m->host, m->port, m->keepalive);
	if (ret != MOSQ_ERR_SUCCESS)
		goto mosquitto_error;

	// Add client to global list of MQTT clients
	// so that thread can call mosquitto loop for this client
	vlist_push(&clients, n);

	return 0;

mosquitto_error:
	n->logger->warn("{}", mosquitto_strerror(ret));

	return ret;
}

int mqtt_stop(struct vnode *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	// Unregister client from global MQTT client list
	// so that mosquitto loop is no longer invoked  for this client
	// important to do that before disconnecting from broker, otherwise, mosquitto thread will attempt to reconnect
	vlist_remove(&clients, vlist_index(&clients, n));

	ret = mosquitto_disconnect(m->client);
	if (ret != MOSQ_ERR_SUCCESS)
		goto mosquitto_error;

	return 0;

mosquitto_error:
	n->logger->warn("{}", mosquitto_strerror(ret));

	return ret;
}

int mqtt_type_start(villas::node::SuperNode *sn)
{
	int ret;

	logger = logging.get("node:mqtt");

	ret = vlist_init(&clients);
	if (ret)
		return ret;

	ret = mosquitto_lib_init();
	if (ret != MOSQ_ERR_SUCCESS)
		goto mosquitto_error;

	// Start thread here to run mosquitto loop for registered clients
	ret = pthread_create(&thread, nullptr, mosquitto_loop_thread, nullptr);
	if (ret)
		return ret;

	return 0;

mosquitto_error:
	logger->warn("{}", mosquitto_strerror(ret));

	return ret;
}

int mqtt_type_stop()
{
	int ret;

	// Stop thread here that executes mosquitto loop
	ret = pthread_cancel(thread);
	if (ret)
		return ret;

	logger->debug("Called pthread_cancel() on MQTT communication management thread.");

	ret = pthread_join(thread, nullptr);
	if (ret)
		return ret;

	ret = mosquitto_lib_cleanup();
	if (ret != MOSQ_ERR_SUCCESS)
		goto mosquitto_error;

	// When this is called the list of clients should be empty
	if (vlist_length(&clients) > 0)
		throw RuntimeError("List of MQTT clients contains elements at time of destruction. Call node_stop for each MQTT node before stopping node type!");

	ret = vlist_destroy(&clients, nullptr, false);
	if (ret)
		return ret;

	return 0;

mosquitto_error:
	logger->warn("{}", mosquitto_strerror(ret));

	return ret;
}

int mqtt_read(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	int pulled;
	struct mqtt *m = (struct mqtt *) n->_vd;
	struct sample *smpt[cnt];

	pulled = queue_signalled_pull_many(&m->queue, (void **) smpt, cnt);

	sample_copy_many(smps, smpt, pulled);
	sample_decref_many(smpt, pulled);

	return pulled;
}

int mqtt_write(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	size_t wbytes;

	char data[1500];

	ret = m->formatter->sprint(data, sizeof(data), &wbytes, smps, cnt);
	if (ret < 0)
		return ret;

	if (m->publish) {
		ret = mosquitto_publish(m->client, nullptr /* mid */, m->publish, wbytes, data, m->qos, m->retain);
		if (ret != MOSQ_ERR_SUCCESS) {
			n->logger->warn("Publish failed: {}", mosquitto_strerror(ret));
			return -abs(ret);
		}
	}
	else
		n->logger->warn("No publish possible because no publish topic is configured");

	return cnt;
}

int mqtt_poll_fds(struct vnode *n, int fds[])
{
	struct mqtt *m = (struct mqtt *) n->_vd;

	fds[0] = queue_signalled_fd(&m->queue);

	return 1;
}

static struct vnode_type p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "mqtt";
	p.description	= "Message Queuing Telemetry Transport (libmosquitto)";
	p.vectorize	= 0;
	p.size		= sizeof(struct mqtt);
	p.type.start	= mqtt_type_start;
	p.type.stop	= mqtt_type_stop;
	p.destroy	= mqtt_destroy;
	p.prepare	= mqtt_prepare;
	p.parse		= mqtt_parse;
	p.check		= mqtt_check;
	p.prepare	= mqtt_prepare;
	p.print		= mqtt_print;
	p.init		= mqtt_init;
	p.destroy	= mqtt_destroy;
	p.start		= mqtt_start;
	p.stop		= mqtt_stop;
	p.read		= mqtt_read;
	p.write		= mqtt_write;
	p.reverse	= mqtt_reverse;
	p.poll_fds	= mqtt_poll_fds;

	if (!node_types)
		node_types = new NodeTypeList();

	node_types->push_back(&p);
}
