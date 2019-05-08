/** Node type: mqtt_unthreaded
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

#include <villas/nodes/mqtt_unthreaded.hpp>
#include <villas/plugin.h>
#include <villas/utils.hpp>
#include <villas/format_type.h>

static void mqtt_unthreaded_log_cb(struct mosquitto *mosq, void *userdata, int level, const char *str)
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


static void mqtt_unthreaded_connect_cb(struct mosquitto *mosq, void *userdata, int result)
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
	
	mqtt_unthreaded_loop(n);
}

static void mqtt_unthreaded_message_cb(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg)
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
	mqtt_unthreaded_loop(n);
}

static void mqtt_unthreaded_disconnect_cb(struct mosquitto *mosq, void *userdata, int result)
{
        struct node *n = (struct node *) userdata;
        struct mqtt *m = (struct mqtt *) n->_vd;

        info("MQTT: Node %s disconnected from broker %s", node_name(n), m->host);
}


static void mqtt_unthreaded_subscribe_cb(struct mosquitto *mosq, void *userdata, int mid, int qos_count, const int *granted_qos)
{
	struct node *n = (struct node *) userdata;
	struct mqtt *m = (struct mqtt *) n->_vd;

	info("MQTT: Node %s subscribed to broker %s", node_name(n), m->host);
	n->state=STATE_STARTED;
}

int mqtt_unthreaded_check(struct node *n)
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



int mqtt_unthreaded_start(struct node *n)
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

	mosquitto_log_callback_set(m->client, mqtt_unthreaded_log_cb);
	mosquitto_connect_callback_set(m->client, mqtt_unthreaded_connect_cb);
	mosquitto_disconnect_callback_set(m->client, mqtt_unthreaded_disconnect_cb);
	mosquitto_message_callback_set(m->client, mqtt_unthreaded_message_cb);
	mosquitto_subscribe_callback_set(m->client, mqtt_unthreaded_subscribe_cb);

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
	
	//The following while loop exists upon completion of subscibe procedure
	while(n->state == STATE_PREPARED){
		mqtt_unthreaded_loop(n);
	}

	return 0;

mosquitto_error:
	warning("MQTT: %s", mosquitto_strerror(ret));

	return ret;
}

int mqtt_unthreaded_stop(struct node *n)
{
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;

	ret = mosquitto_disconnect(m->client);
	if (ret)
		goto mosquitto_error;

	ret = io_destroy(&m->io);
	if (ret)
		return ret;

	return 0;

mosquitto_error:
	warning("MQTT: %s", mosquitto_strerror(ret));

	return ret;
}

int mqtt_unthreaded_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int pulled=0;	
	mqtt_unthreaded_loop(n);

	if(cnt > 0){
		struct mqtt *m = (struct mqtt *) n->_vd;
		struct sample *smpt[cnt];
		pulled = queue_signalled_pull_many(&m->queue, (void **) smpt, cnt);

		sample_copy_many(smps, smpt, pulled);
		sample_decref_many(smpt, pulled);
	}
	return pulled;
}

int mqtt_unthreaded_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
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
	
	mqtt_unthreaded_loop(n);
	
	return cnt;
}

int mqtt_unthreaded_loop(struct node *n){
		
	int ret;
	struct mqtt *m = (struct mqtt *) n->_vd;
	
	//Carry out network operations in a synchronous way.
        ret = mosquitto_loop(m->client, 0, 1);
        if(ret){
                warning("MQTT: connection error for node %s: %s, attempting reconnect", node_name(n), mosquitto_strerror(ret));
                ret = mosquitto_reconnect(m->client);
                if(ret != MOSQ_ERR_SUCCESS){
                        error("MQTT: reconnection to broker failed for node %s: %s", node_name(n), mosquitto_strerror(ret));
                }
                else{
                        warning("MQTT: successfully reconnected to broker for node %s: %s", node_name(n), mosquitto_strerror(ret));
                }
                ret = mosquitto_loop(m->client, -1, 1);
		if(ret != MOSQ_ERR_SUCCESS){
			error("MQTT: persisting connection error for node %s: %s", node_name(n), mosquitto_strerror(ret));
		}
        }
	
	return 0;
}

int mqtt_unthreaded_poll_fds(struct node *n, int fds[])
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

	p.name			= "mqtt_unthreaded";
	p.description		= "Message Queuing Telemetry Transport (libmosquitto), unthreaded variant";
	p.type			= PLUGIN_TYPE_NODE;
	p.node.instances.state	= STATE_DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct mqtt);
	p.node.type.start	= mqtt_type_start;
	p.node.type.stop	= mqtt_type_stop;
	p.node.destroy		= mqtt_destroy;
	p.node.parse		= mqtt_parse;
	p.node.check		= mqtt_unthreaded_check;
	p.node.print		= mqtt_print;
	p.node.start		= mqtt_unthreaded_start;
	p.node.stop		= mqtt_unthreaded_stop;
	p.node.read		= mqtt_unthreaded_read;
	p.node.write		= mqtt_unthreaded_write;
	p.node.reverse		= mqtt_reverse;
	p.node.poll_fds		= mqtt_unthreaded_poll_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	if (plugins.state != STATE_DESTROYED)
		vlist_remove_all(&plugins, &p);
}
