/** Node type: mqtt
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/nodes/mqtt.h>
#include <villas/plugin.h>
#include <villas/utils.h>
#include <villas/io_format.h>

#ifdef MQTT_THREAD
#include <pthread.h>

static struct {
	pthread_mutex_t mutex;
	int length;
	struct pollfd *fds;
	struct mosquitto **clients;
} polling = {
	.length = 0,
	.fds = NULL,
	.clients = NULL
};

static pthread_t thread;

static int mqtt_register_client(struct mosquitto *mosq)
{
//	pthread_mutex_lock(&polling.mutex);

	/* Add this client to the pollfd list */
	int i = polling.length++;

	polling.fds     = realloc(polling.fds,     polling.length * sizeof(struct pollfd));
	polling.clients = realloc(polling.clients, polling.length * sizeof(struct mosquitto *));

	if (!polling.fds || !polling.clients) {
//	pthread_mutex_unlock(&polling.mutex);
		return -1;
	}

	polling.clients[i] = mosq;
	polling.fds[i].events = POLLIN;
	polling.fds[i].fd = mosquitto_socket(mosq);

//	pthread_mutex_unlock(&polling.mutex);

	return 0;
}

static int mqtt_unregister_client(struct mosquitto *mosq)
{
//	pthread_mutex_lock(&polling.mutex);

	/* Find client */
	int i;
	for (i = 0; i < polling.length; i++) {
		if (polling.clients[i] == mosq)
			break;
	}

	if (i >= polling.length) {
//		pthread_mutex_unlock(&polling.mutex);
		return -1; /* Otherwise something wrong happened! */
	}

	/* Remove this client to the pollfd list */
	memmove(polling.fds     + i, polling.fds     + i + 1, (polling.length - i - 1) * sizeof(struct pollfd));
	memmove(polling.clients + i, polling.clients + i + 1, (polling.length - i - 1) * sizeof(struct mosquitto *));

	polling.length--;

//	pthread_mutex_unlock(&polling.mutex);

	return 0;
}

static void * mqtt_thread(void *ctx)
{
	int ret;

	debug(1, "MQTT: started thread");

	while (1) {
//		pthread_mutex_lock(&polling.mutex);

		debug(1, "MQTT: Polling on %d clients", polling.length);

		ret = poll(polling.fds, polling.length, -1);
		if (ret < 0)
			serror("Failed to poll");

		for (int i = 0; i < polling.length; i++) {
			if (polling.fds[i].revents & POLLIN) {
				ret = mosquitto_loop(polling.clients[i], -1, 1);
				if (ret)
					warn("MQTT: Loop failed for clients %p!", polling.clients[i]);
			}
		}

//		pthread_mutex_unlock(&polling.mutex);
	}

	return NULL;
}
#endif

static void mqtt_log_cb(struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	switch (level) {
		case MOSQ_LOG_NONE:
		case MOSQ_LOG_INFO:
		case MOSQ_LOG_NOTICE:
			info("MQTT: %s", str);
			break;

		case MOSQ_LOG_WARNING:
			warn("MQTT: %s", str);
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

	debug(1, "MQTT: Node %s connected to broker %s", node_name(n), m->host);

//	mqtt_register_client(mosq);
}

static void mqtt_disconnect_cb(struct mosquitto *mosq, void *userdata, int result)
{
	struct node *n = (struct node *) userdata;
	struct mqtt *m = (struct mqtt *) n->_vd;

	debug(1, "MQTT: Node %s disconnected from broker %s", node_name(n), m->host);

//	mqtt_unregister_client(mosq)
}

static void mqtt_message_cb(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg)
{
	int ret;
	struct node *n = (struct node *) userdata;
	struct mqtt *m = (struct mqtt *) n->_vd;
	struct sample *smp;

	debug(1, "MQTT: Node %s received a message of %d bytes from broker %s", node_name(n), msg->payloadlen, m->host);

	smp = sample_alloc(&m->pool);
	if (!smp) {
		warn("Pool underrun in subscriber of %s", node_name(n));
		return;
	}

	ret = io_format_sscan(m->format, msg->payload, msg->payloadlen, NULL, &smp, 1, 0);
	if (ret != 1)
		return;

	queue_signalled_push(&m->queue, (void *) smp);
}

static void mqtt_subscribe_cb(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	struct node *n = (struct node *) userdata;
	struct mqtt *m = (struct mqtt *) n->_vd;

	info("MQTT node %s subscribed to broker %s", node_name(n), m->host);
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
	const char *format = "villas-human";
	const char *publish = NULL;
	const char *subscribe = NULL;
	const char *username = NULL;
	const char *password = NULL;

	/* Default values */
	m->port = 1883;
	m->qos = 0;
	m->retain = 0;
	m->keepalive = 1; // 1 second
	m->ssl.enabled = 0;
	m->ssl.insecure = 0;

	json_error_t err;
	json_t *json_ssl = NULL;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s, s?: s, s: s, s?: i, s?: i, s?: i, s?: b, s?: s, s?: s, s?: o }",
		"publish", &publish,
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
	m->publish = publish ? strdup(publish) : NULL;
	m->subscribe = subscribe ? strdup(subscribe) : NULL;
	m->username = username ? strdup(username) : NULL;
	m->password = password ? strdup(password) : NULL;

	if (json_ssl) {
		const char *cafile = NULL;
		const char *capath = NULL;
		const char *certfile = NULL;
		const char *keyfile = NULL;

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

		m->ssl.cafile = cafile ? strdup(cafile) : NULL;
		m->ssl.capath = capath ? strdup(capath) : NULL;
		m->ssl.certfile = certfile ? strdup(certfile) : NULL;
		m->ssl.keyfile = keyfile ? strdup(keyfile) : NULL;
	}

	m->format = io_format_lookup(format);
	if (!m->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

	return 0;
}

char * mqtt_print(struct node *n)
{
	struct mqtt *m = (struct mqtt *) n->_vd;

	char *buf = NULL;

	strcatf(&buf, "format=%s, host=%s, port=%d, username=%s, keepalive=%s, ssl=%s", plugin_name(m->format),
		m->host,
		m->port,
		m->username,
		m->keepalive ? "yes" : "no",
		m->ssl.enabled ? "yes" : "no"
	);

	if (m->publish)
		strcatf(&buf, ", publish=%s", m->publish);

	if (m->subscribe)
		strcatf(&buf, ", subscribe=%s", m->subscribe);

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

#ifdef MQTT_THREAD
	ret = mosquitto_threaded_set(m->client, 1);
	if (ret)
		return ret;
#endif

	if (m->username && m->password) {
		ret = mosquitto_username_pw_set(m->client, m->username, m->password);
		if (ret)
			return ret;
	}

	if (m->ssl.enabled) {
		ret = mosquitto_tls_set(m->client, m->ssl.cafile, m->ssl.capath, m->ssl.certfile, m->ssl.keyfile, NULL);
		if (ret)
			return ret;

		ret = mosquitto_tls_insecure_set(m->client, m->ssl.insecure);
		if (ret)
			return ret;
	}

	mosquitto_log_callback_set(m->client, mqtt_log_cb);
	mosquitto_connect_callback_set(m->client, mqtt_connect_cb);
	mosquitto_disconnect_callback_set(m->client, mqtt_disconnect_cb);
	mosquitto_message_callback_set(m->client, mqtt_message_cb);
	mosquitto_subscribe_callback_set(m->client, mqtt_subscribe_cb);

	ret = pool_init(&m->pool, 1024, SAMPLE_LEN(n->samplelen), &memtype_hugepage);
	if (ret)
		return ret;

	ret = queue_signalled_init(&m->queue, 1024, &memtype_hugepage, 0);
	if (ret)
		return ret;

	ret = mosquitto_connect(m->client, m->host, m->port, m->keepalive);
	if (ret)
		return ret;

#ifdef MQTT_THREAD
	mqtt_register_client(m->client);
#else
	ret = mosquitto_loop_start(m->client);
	if (ret)
		return ret;
#endif

	ret = mosquitto_subscribe(m->client, NULL, m->subscribe, m->qos);
	if (ret)
		return ret;

	return 0;
}

int mqtt_stop(struct node *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	ret = mosquitto_disconnect(m->client);
	if (ret)
		return ret;

#ifdef MQTT_THREAD
	mqtt_unregister_client(m->client);
#else
	ret = mosquitto_loop_stop(m->client, 0);
	if (ret)
		return ret;
#endif

	return 0;
}

int mqtt_init()
{
	int ret;

	ret = mosquitto_lib_init();
	if (ret)
		return ret;

#ifdef MQTT_THREAD
	ret = pthread_mutex_init(&polling.mutex, NULL);
	if (ret)
		return ret;

	ret = pthread_create(&thread, NULL, mqtt_thread, NULL);
	if (ret)
		return ret;
#endif

	return 0;
}

int mqtt_deinit()
{
	int ret;

#ifdef MQTT_THREAD
	ret = pthread_cancel(thread);
	if (ret)
		return ret;

	ret = pthread_join(thread, NULL);
	if (ret)
		return ret;

	ret = pthread_mutex_destroy(&polling.mutex);
	if (ret)
		return ret;
#endif

	ret = mosquitto_lib_cleanup();
	if (ret)
		return ret;

	return 0;
}

int mqtt_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int pulled;
	struct mqtt *m = (struct mqtt *) n->_vd;
	struct sample *smpt[cnt];

	pulled = queue_signalled_pull_many(&m->queue, (void **) smpt, cnt);

	sample_copy_many(smps, smpt, pulled);

	return pulled;
}

int mqtt_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	size_t wbytes;

	char data[1500];

	ret = io_format_sprint(m->format, data, sizeof(data), &wbytes, smps, cnt, SAMPLE_HAS_ALL);
	if (ret <= 0)
		return -1;

	ret = mosquitto_publish(m->client, NULL /* mid */, m->publish, wbytes, data, m->qos, m->retain);
	if (ret)
		return -1;

	return cnt;
}

int mqtt_fd(struct node *n)
{
	struct mqtt *m = (struct mqtt *) n->_vd;

	return queue_signalled_fd(&m->queue);
}

static struct plugin p = {
	.name		= "mqtt",
	.description	= "Message Queuing Telemetry Transport (libmosquitto)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct mqtt),
		.reverse	= mqtt_reverse,
		.parse		= mqtt_parse,
		.print		= mqtt_print,
		.start		= mqtt_start,
		.destroy	= mqtt_destroy,
		.stop		= mqtt_stop,
		.init		= mqtt_init,
		.deinit		= mqtt_deinit,
		.read		= mqtt_read,
		.write		= mqtt_write,
		.fd		= mqtt_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
