/** Node type: mqtt
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <string.h>
#include <mosquitto.h>

#include <villas/nodes/mqtt.hpp>
#include <villas/plugin.h>
#include <villas/utils.hpp>
#include <villas/format_type.h>

// Each process has a list of clients for which a thread invokes the mosquitto loop
static struct vlist clients;
static pthread_t thread;

static void * mosquitto_loop_thread(void *ctx)
{
    int ret;
    while(true){
        for (unsigned i = 0; i < vlist_length(&clients); i++) {
            struct node *node = (struct node *) vlist_at(&clients, i);
            struct mqtt *m = (struct mqtt *) node->_vd;

            // execute mosquitto loop for this client
            ret = mosquitto_loop(m->client, 0, 1);
            if(ret){
                warning("MQTT: connection error for node %s: %s, attempting reconnect", node_name(node), mosquitto_strerror(ret));
                ret = mosquitto_reconnect(m->client);
                if(ret != MOSQ_ERR_SUCCESS){
                    error("MQTT: reconnection to broker failed for node %s: %s", node_name(node), mosquitto_strerror(ret));
                }
                else{
                    warning("MQTT: successfully reconnected to broker for node %s: %s", node_name(node), mosquitto_strerror(ret));
                }
                ret = mosquitto_loop(m->client, -1, 1);
                if(ret != MOSQ_ERR_SUCCESS){
                    error("MQTT: persisting connection error for node %s: %s", node_name(node), mosquitto_strerror(ret));
                }
            }
        } // for loop
    } // while(1)

    return nullptr;
}

static void mqtt_log_cb(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	switch (level) {
		case MOSQ_LOG_NONE:
		case MOSQ_LOG_INFO:
		case MOSQ_LOG_NOTICE:
			info("MQTT: %s", str);
			break;

		case MOSQ_LOG_WARNING:
			warning("MQTT: %s", str);
			break;

		case MOSQ_LOG_ERR:
			error("MQTT: %s", str);
			break;

		case MOSQ_LOG_DEBUG:
			debug(5, "MQTT: %s", str);
			break;
	}
}

static void mqtt_connect_cb(struct mosquitto *mosq, void *userdata, int result)
{
	struct node *n = (struct node *) userdata;
	struct mqtt *m = (struct mqtt *) n->_vd;

	int ret;

	info("MQTT: Node %s connected to broker %s", node_name(n), m->host);

	if (m->subscribe) {
		ret = mosquitto_subscribe(m->client, nullptr, m->subscribe, m->qos);
		if (ret)
			warning("MQTT: failed to subscribe to topic '%s' for node %s: %s", m->subscribe, node_name(n), mosquitto_strerror(ret));
	}
	else
		warning("MQTT: no subscribe for node %s as no subscribe topic is given", node_name(n));
}

static void mqtt_disconnect_cb(struct mosquitto *mosq, void *userdata, int result)
{
	struct node *n = (struct node *) userdata;
	struct mqtt *m = (struct mqtt *) n->_vd;

	info("MQTT: Node %s disconnected from broker %s", node_name(n), m->host);
}

static void mqtt_message_cb(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg)
{
	int ret;
	struct node *n = (struct node *) userdata;
	struct mqtt *m = (struct mqtt *) n->_vd;
	struct sample *smps[n->in.vectorize];

	debug(5, "MQTT: Node %s received a message of %d bytes from broker %s", node_name(n), msg->payloadlen, m->host);

	ret = sample_alloc_many(&m->pool, smps, n->in.vectorize);
	if (ret <= 0) {
		warning("Pool underrun in subscriber of %s", node_name(n));
		return;
	}

	ret = io_sscan(&m->io, (char *) msg->payload, msg->payloadlen, nullptr, smps, n->in.vectorize);
	if (ret < 0) {
		warning("MQTT: Node %s received an invalid message", node_name(n));
		warning("  Payload: %s", (char *) msg->payload);
		return;
	}
	if (ret == 0) {
		debug(4, "MQTT: skip empty message for node %s", node_name(n));
		sample_decref_many(smps, n->in.vectorize);
		return;
	}

	queue_signalled_push_many(&m->queue, (void **) smps, n->in.vectorize);
}

static void mqtt_subscribe_cb(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	struct node *n = (struct node *) userdata;
	struct mqtt *m = (struct mqtt *) n->_vd;

	info("MQTT: Node %s subscribed to broker %s", node_name(n), m->host);
}

int mqtt_reverse(struct node *n)
{
	struct mqtt *m = (struct mqtt *) n->_vd;

	SWAP(m->publish, m->subscribe);

	return 0;
}

int mqtt_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	const char *host;
	const char *format = "villas.binary";
	const char *publish = nullptr;
	const char *subscribe = nullptr;
	const char *username = nullptr;
	const char *password = nullptr;

	/* Default values */
	m->port = 1883;
	m->qos = 0;
	m->retain = 0;
	m->keepalive = 1; /* 1 second */
	m->ssl.enabled = 0;
	m->ssl.insecure = 0;

	json_error_t err;
	json_t *json_ssl = nullptr;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: { s?: s }, s?: { s?: s }, s?: s, s: s, s?: i, s?: i, s?: i, s?: b, s?: s, s?: s, s?: o }",
		"out",
			"publish", &publish,
		"in",
			"subscribe", &subscribe,
		"format", &format,
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
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	m->host = strdup(host);
	m->publish = publish ? strdup(publish) : nullptr;
	m->subscribe = subscribe ? strdup(subscribe) : nullptr;
	m->username = username ? strdup(username) : nullptr;
	m->password = password ? strdup(password) : nullptr;

	if (!m->publish && !m->subscribe)
		error("At least one topic has to be specified for node %s", node_name(n));

	if (json_ssl) {
		const char *cafile = nullptr;
		const char *capath = nullptr;
		const char *certfile = nullptr;
		const char *keyfile = nullptr;

		ret = json_unpack_ex(cfg, &err, 0, "{ s?: b, s?: b, s?: s, s?: s, s?: s, s?: s }",
			"enabled", &m->ssl.enabled,
			"insecure", &m->ssl.insecure,
			"cafile", &cafile,
			"capath", &capath,
			"certfile", &certfile,
			"keyfile", &keyfile
		);
		if (ret)
			jerror(&err, "Failed to parse SSL configuration of node %s", node_name(n));

		if (m->ssl.enabled && !cafile && !capath)
			error("Either 'ssl.cafile' or 'ssl.capath' settings must be set for node %s.", node_name(n));

		m->ssl.cafile = cafile ? strdup(cafile) : nullptr;
		m->ssl.capath = capath ? strdup(capath) : nullptr;
		m->ssl.certfile = certfile ? strdup(certfile) : nullptr;
		m->ssl.keyfile = keyfile ? strdup(keyfile) : nullptr;
	}

	m->format = format_type_lookup(format);
	if (!m->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	return 0;
}

int mqtt_check(struct node *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	ret = mosquitto_sub_topic_check(m->subscribe);
	if (ret != MOSQ_ERR_SUCCESS)
		error("Invalid subscribe topic: '%s' for node %s: %s", m->subscribe, node_name(n), mosquitto_strerror(ret));

	ret = mosquitto_pub_topic_check(m->publish);
	if (ret != MOSQ_ERR_SUCCESS)
		error("Invalid publish topic: '%s' for node %s: %s", m->publish, node_name(n), mosquitto_strerror(ret));

	return 0;
}

char * mqtt_print(struct node *n)
{
	struct mqtt *m = (struct mqtt *) n->_vd;

	char *buf = nullptr;

	strcatf(&buf, "format=%s, host=%s, port=%d, keepalive=%s, ssl=%s", format_type_name(m->format),
		m->host,
		m->port,
		m->keepalive ? "yes" : "no",
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

int mqtt_destroy(struct node *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

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

	free(m->host);

	return 0;
}

int mqtt_start(struct node *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	m->client = mosquitto_new(n->name, 0, (void *) n);
	if (!m->client)
		return -1;

	if (m->username && m->password) {
		ret = mosquitto_username_pw_set(m->client, m->username, m->password);
		if (ret)
			goto mosquitto_error;
	}

	if (m->ssl.enabled) {
		ret = mosquitto_tls_set(m->client, m->ssl.cafile, m->ssl.capath, m->ssl.certfile, m->ssl.keyfile, nullptr);
		if (ret)
			goto mosquitto_error;

		ret = mosquitto_tls_insecure_set(m->client, m->ssl.insecure);
		if (ret)
			goto mosquitto_error;
	}

	mosquitto_log_callback_set(m->client, mqtt_log_cb);
	mosquitto_connect_callback_set(m->client, mqtt_connect_cb);
	mosquitto_disconnect_callback_set(m->client, mqtt_disconnect_cb);
	mosquitto_message_callback_set(m->client, mqtt_message_cb);
	mosquitto_subscribe_callback_set(m->client, mqtt_subscribe_cb);

	ret = io_init(&m->io, m->format, &n->in.signals, SAMPLE_HAS_ALL & ~SAMPLE_HAS_OFFSET);
	if (ret)
		return ret;

	ret = io_check(&m->io);
	if (ret)
		return ret;

	ret = pool_init(&m->pool, 1024, SAMPLE_LENGTH(vlist_length(&n->in.signals)), &memory_hugepage);
	if (ret)
		return ret;

	ret = queue_signalled_init(&m->queue, 1024, &memory_hugepage, 0);
	if (ret)
		return ret;

	ret = mosquitto_connect(m->client, m->host, m->port, m->keepalive);
	if (ret)
		goto mosquitto_error;

	// add client to global list of MQTT clients
	// so that thread can call mosquitto loop for this client
    vlist_push(&clients, n);

	return 0;

mosquitto_error:
	warning("MQTT: %s", mosquitto_strerror(ret));

	return ret;
}

int mqtt_stop(struct node *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	ret = mosquitto_disconnect(m->client);
	if (ret)
		goto mosquitto_error;

	// unregister client from global MQTT client list
	// so that mosquitto loop is no longer invoked  for this client
	vlist_remove_all(&clients, n);

	ret = io_destroy(&m->io);
	if (ret)
		return ret;

	return 0;

mosquitto_error:
	warning("MQTT: %s", mosquitto_strerror(ret));

	return ret;
}

int mqtt_type_start(villas::node::SuperNode *sn)
{
	int ret;

    ret = vlist_init(&clients);
    if (ret) {
        return ret;
    }

	ret = mosquitto_lib_init();
	if (ret)
		goto mosquitto_error;

	// start thread here to run mosquitto loop for registered clients
    ret = pthread_create(&thread, nullptr, mosquitto_loop_thread, nullptr);
    if (ret) {
        return ret;
    }

	return 0;

mosquitto_error:
	warning("MQTT: %s", mosquitto_strerror(ret));

	return ret;
}

int mqtt_type_stop()
{
	int ret;

	// stop thread here that executes mosquitto loop
    ret = pthread_cancel(thread);
    if (ret)
        return ret;

    ret = pthread_join(thread, nullptr);
    if (ret) {
        return ret;
    }

	ret = mosquitto_lib_cleanup();
	if (ret)
		goto mosquitto_error;

	// when this is called the list of clients should be empty
	if (vlist_length(&clients) > 0) {
	    error("List of MQTT clients contains elements at time of destruction. Call node_stop for each MQTT node before stopping node type!");
	}
    vlist_destroy(&clients, nullptr, false);

	return 0;

mosquitto_error:
	warning("MQTT: %s", mosquitto_strerror(ret));

	return ret;
}

int mqtt_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int pulled;
	struct mqtt *m = (struct mqtt *) n->_vd;
	struct sample *smpt[cnt];

	pulled = queue_signalled_pull_many(&m->queue, (void **) smpt, cnt);

	sample_copy_many(smps, smpt, pulled);
	sample_decref_many(smpt, pulled);

	return pulled;
}

int mqtt_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	size_t wbytes;

	char data[1500];

	ret = io_sprint(&m->io, data, sizeof(data), &wbytes, smps, cnt);
	if (ret < 0)
		return ret;

	if (m->publish) {
		ret = mosquitto_publish(m->client, nullptr /* mid */, m->publish, wbytes, data, m->qos, m->retain);
		if (ret != MOSQ_ERR_SUCCESS) {
			warning("MQTT: publish failed for node %s: %s", node_name(n), mosquitto_strerror(ret));
			return -abs(ret);
		}
	}
	else
		warning("MQTT: no publish for node %s possible because no publish topic is given", node_name(n));

	return cnt;
}

int mqtt_poll_fds(struct node *n, int fds[])
{
	struct mqtt *m = (struct mqtt *) n->_vd;

	fds[0] = queue_signalled_fd(&m->queue);

	return 1;
}

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	if (plugins.state == STATE_DESTROYED)
		vlist_init(&plugins);

	p.name			= "mqtt";
	p.description		= "Message Queuing Telemetry Transport (libmosquitto)";
	p.type			= PLUGIN_TYPE_NODE;
	p.node.instances.state	= STATE_DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct mqtt);
	p.node.type.start	= mqtt_type_start;
	p.node.type.stop	= mqtt_type_stop;
	p.node.destroy		= mqtt_destroy;
	p.node.parse		= mqtt_parse;
	p.node.check		= mqtt_check;
	p.node.print		= mqtt_print;
	p.node.start		= mqtt_start;
	p.node.stop		= mqtt_stop;
	p.node.read		= mqtt_read;
	p.node.write		= mqtt_write;
	p.node.reverse		= mqtt_reverse;
	p.node.poll_fds		= mqtt_poll_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	if (plugins.state != STATE_DESTROYED)
		vlist_remove_all(&plugins, &p);
}
