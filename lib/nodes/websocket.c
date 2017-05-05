/** Node type: Websockets (libwebsockets)
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <libconfig.h>

#include "super_node.h"
#include "webmsg.h"
#include "webmsg_format.h"
#include "timing.h"
#include "utils.h"
#include "plugin.h"

#include "nodes/websocket.h"

/* Private static storage */
static struct list connections = { .state = STATE_DESTROYED };	/**< List of active libwebsocket connections which receive samples from all nodes (catch all) */
static struct web *web;

/* Forward declarations */
static struct plugin p;

static char * websocket_connection_name(struct websocket_connection *c)
{
	if (!c->_name) {
		strcatf(&c->_name, "%s (%s)", c->peer.name, c->peer.ip);

		if (c->node)
			strcatf(&c->_name, " for node %s", node_name(c->node));

		strcatf(&c->_name, " in %s mode", c->mode == WEBSOCKET_MODE_CLIENT ? "client" : "server");
	}

	return c->_name;
}

static int websocket_connection_init(struct websocket_connection *c, struct lws *wsi)
{
	int ret;

	lws_get_peer_addresses(wsi, lws_get_socket_fd(wsi), c->peer.name, sizeof(c->peer.name), c->peer.ip, sizeof(c->peer.ip));

	info("LWS: New connection %s", websocket_connection_name(c));

	c->state = STATE_INITIALIZED;
	c->wsi = wsi;

	if (c->node) {
		struct websocket *w = c->node->_vd;

		list_push(&w->connections, c);
	}
	else
		list_push(&connections, c);

	ret = queue_init(&c->queue, DEFAULT_QUEUELEN, &memtype_hugepage);
	if (ret)
		return ret;

	return 0;
}

static int websocket_connection_destroy(struct websocket_connection *c)
{
	int ret;

	if (c->state == STATE_DESTROYED)
		return 0;

	info("LWS: Connection %s closed", websocket_connection_name(c));

	if (c->node) {
		struct websocket *w = c->node->_vd;
		list_remove(&w->connections, c);
	}
	else
		list_remove(&connections, c);

	if (c->_name)
		free(c->_name);

	ret = queue_destroy(&c->queue);
	if (ret)
		return ret;

	c->state = STATE_DESTROYED;
	c->wsi = NULL;

	return ret;
}

static void websocket_destination_destroy(struct websocket_destination *d)
{
	free(d->uri);

	free((char *) d->info.path);
	free((char *) d->info.address);
}

static int websocket_connection_write(struct websocket_connection *c, struct sample *smps[], unsigned cnt)
{
	int ret;

	switch (c->state) {
		case STATE_INITIALIZED:
			c->state = STATE_STARTED;
			/* fall through */

		case STATE_STARTED:
			for (int i = 0; i < cnt; i++) {
				sample_get(smps[i]); /* increase reference count */

				ret = queue_push(&c->queue, (void **) smps[i]);
				if (ret != 1)
					warn("Queue overrun in websocket connection: %s", websocket_connection_name(c));
			}

			lws_callback_on_writable(c->wsi);
			break;

		default: { }
	}

	return 0;
}

static void websocket_connection_close(struct websocket_connection *c, struct lws *wsi, enum lws_close_status status, const char *reason)
{
	lws_close_reason(wsi, status, (unsigned char *) reason, strlen(reason));

	char *msg = strf("LWS: Closing connection");

	if (c)
		msg = strcatf(&msg, " with %s", websocket_connection_name(c));

	msg = strcatf(&msg, ": status=%u, reason=%s", status, reason);

	warn(msg);

	free(msg);
}

int websocket_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int ret;
	struct websocket_connection *c = user;

	struct webmsg *msg;
	struct sample *smp;

	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			ret = websocket_connection_init(c, wsi);
			if (ret)
				return -1;

			return 0;

		case LWS_CALLBACK_ESTABLISHED:
			c->state = STATE_DESTROYED;
			c->mode = WEBSOCKET_MODE_SERVER;

			/* Get path of incoming request */
			char uri[64];
			lws_hdr_copy(wsi, uri, sizeof(uri), WSI_TOKEN_GET_URI); /* The path component of the*/
			if (strlen(uri) <= 0) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, "Invalid URL");
				return -1;
			}

			if ((uri[0] == '/' && uri[1] == 0) || uri[0] == 0){
				/* Catch all connection */
				c->node = NULL;
			}
			else {
				char *node = uri + 1;

				/* Search for node whose name matches the URI. */
				c->node = list_lookup(&p.node.instances, node);
				if (c->node == NULL) {
					websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, "Unknown node");
					return -1;
				}
			}

			ret = websocket_connection_init(c, wsi);
			if (ret)
				return -1;

			return 0;

		case LWS_CALLBACK_CLOSED:
			websocket_connection_destroy(c);

			if (c->mode == WEBSOCKET_MODE_CLIENT)
				free(c);

			return 0;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
		case LWS_CALLBACK_SERVER_WRITEABLE:
			if (c->state == STATE_STOPPED) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_NORMAL, "Goodbye");
				return -1;
			}

			if (c->node && c->node->state != STATE_STARTED) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_GOINGAWAY, "Node stopped");
				return -1;
			}

			char *buf = NULL;

			while (queue_pull(&c->queue, (void **) &smp)) {
				buf = realloc(buf, LWS_PRE + WEBMSG_LEN(smp->length));
				if (!buf)
					serror("realloc failed:");

				msg = (struct webmsg *) (buf + LWS_PRE);

				msg->version  = WEBMSG_VERSION;
				msg->type     = WEBMSG_TYPE_DATA;
				msg->length   = smp->length;
				msg->sequence = smp->sequence;
				msg->id       = smp->source->id;
				msg->ts.sec   = smp->ts.origin.tv_sec;
				msg->ts.nsec  = smp->ts.origin.tv_nsec;

				memcpy(&msg->data, &smp->data, SAMPLE_DATA_LEN(smp->length));

				webmsg_hton(msg);

				sample_put(smp);

				ret = lws_write(wsi, (unsigned char *) msg, WEBMSG_LEN(msg->length), LWS_WRITE_BINARY);
				if (ret < 0) {
					warn("Failed lws_write() for connection %s", websocket_connection_name(c));
					return -1;
				}

				if (lws_send_pipe_choked(wsi))
					break;
			}

			free(buf);

			/* There are still samples in the queue */
			if (queue_available(&c->queue) > 0)
				lws_callback_on_writable(wsi);

			return 0;

		case LWS_CALLBACK_CLIENT_RECEIVE:
		case LWS_CALLBACK_RECEIVE:
			if (!lws_frame_is_binary(wsi)) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_UNACCEPTABLE_OPCODE, "Binary data expected");
				return -1;
			}

			if (len < WEBMSG_LEN(0)) {
				websocket_connection_close(c, wsi, LWS_CLOSE_STATUS_PROTOCOL_ERR, "Invalid packet");
				return -1;
			}

			struct timespec ts_recv = time_now();

			msg = (struct webmsg *) in;
			while ((char *) msg + WEBMSG_LEN(msg->length) < (char *) in + len) {
				struct node *dest;

				/* Convert message to host byte-order */
				webmsg_ntoh(msg);

				/* Find destination node of this message */
				if (c->node)
					dest = c->node;
				else {
					dest = NULL;

					for (int i = 0; i < list_length(&p.node.instances); i++) {
						struct node *n = list_at(&p.node.instances, i);

						if (n->id == msg->id) {
							dest = n;
							break;
						}
					}

					if (!dest) {
						warn("Ignoring message due to invalid node id");
						goto next;
					}
				}

				struct websocket *w = dest->_vd;

				ret = sample_alloc(&w->pool, &smp, 1);
				if (ret != 1) {
					warn("Pool underrun for connection: %s", websocket_connection_name(c));
					break;
				}

				smp->ts.origin = WEBMSG_TS(msg);
				smp->ts.received = ts_recv;

				smp->sequence  = msg->sequence;
				smp->length    = msg->length;
				if (smp->length > smp->capacity) {
					smp->length = smp->capacity;
					warn("Dropping values for connection: %s", websocket_connection_name(c));
				}

				memcpy(&smp->data, &msg->data, SAMPLE_DATA_LEN(smp->length));

				ret = queue_signalled_push(&w->queue, (void **) smp);
				if (ret != 1) {
					warn("Queue overrun for connection %s", websocket_connection_name(c));
					break;
				}

				/* Next message */
next:				msg = (struct webmsg *) ((char *) msg + WEBMSG_LEN(msg->length));
			}

			return 0;

		default:
			return 0;
	}
}

int websocket_init(struct super_node *sn)
{
	list_init(&connections);

	web = &sn->web;

	if (web->state != STATE_STARTED)
		return -1;

	return 0;
}

int websocket_deinit()
{
	for (size_t i = 0; i < list_length(&connections); i++) {
		struct websocket_connection *c = list_at(&connections, i);

		c->state = STATE_STOPPED;

		lws_callback_on_writable(c->wsi);
	}

	/* Wait for all connections to be closed */
	while (list_length(&connections) > 0)
		sleep(0.2);

	list_destroy(&connections, (dtor_cb_t) websocket_destination_destroy, true);

	return 0;
}

int websocket_start(struct node *n)
{
	int ret;
	struct websocket *w = n->_vd;

	size_t blocklen = LWS_PRE + WEBMSG_LEN(DEFAULT_WEBSOCKET_SAMPLELEN);

	ret = pool_init(&w->pool, DEFAULT_WEBSOCKET_QUEUELEN, blocklen, &memtype_hugepage);
	if (ret)
		return ret;

	ret = queue_signalled_init(&w->queue, DEFAULT_WEBSOCKET_QUEUELEN, &memtype_hugepage);
	if (ret)
		return ret;

	for (int i = 0; i < list_length(&w->destinations); i++) {
		struct websocket_destination *d = list_at(&w->destinations, i);

		struct websocket_connection *c = alloc(sizeof(struct websocket_connection));

		c->state = STATE_DESTROYED;
		c->mode = WEBSOCKET_MODE_CLIENT;
		c->node = n;

		d->info.context = web->context;
		d->info.vhost = web->vhost;
		d->info.userdata = c;

		lws_client_connect_via_info(&d->info);
	}

	return 0;
}

int websocket_stop(struct node *n)
{
	int ret;
	struct websocket *w = n->_vd;

	for (size_t i = 0; i < list_length(&w->connections); i++) {
		struct websocket_connection *c = list_at(&w->connections, i);

		c->state = STATE_STOPPED;

		lws_callback_on_writable(c->wsi);
	}

	/* Wait for all connections to be closed */
	while (list_length(&w->connections) > 0)
		sleep(1);

	ret = queue_signalled_destroy(&w->queue);
	if (ret)
		return ret;

	ret = pool_destroy(&w->pool);
	if (ret)
		return ret;

	return 0;
}

int websocket_destroy(struct node *n)
{
	struct websocket *w = n->_vd;

	list_destroy(&w->connections, NULL, false);
	list_destroy(&w->destinations, (dtor_cb_t) websocket_destination_destroy, true);

	return 0;
}

int websocket_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int avail;

	struct websocket *w = n->_vd;
	struct sample *cpys[cnt];

	do {
		avail = queue_signalled_pull_many(&w->queue, (void **) cpys, cnt);
		if (avail < 0)
			return avail;
	} while (avail == 0);

	for (int i = 0; i < avail; i++) {
		sample_copy(smps[i], cpys[i]);
		sample_put(cpys[i]);
	}

	return avail;
}

int websocket_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int avail;

	struct websocket *w = n->_vd;
	struct sample *cpys[cnt];

	/* Make copies of all samples */
	avail = sample_alloc(&w->pool, cpys, cnt);
	if (avail < cnt)
		warn("Pool underrun for node %s: avail=%u", node_name(n), avail);

	for (int i = 0; i < avail; i++) {
		sample_copy(cpys[i], smps[i]);

		cpys[i]->source = n;
	}

	for (size_t i = 0; i < list_length(&w->connections); i++) {
		struct websocket_connection *c = list_at(&w->connections, i);

		websocket_connection_write(c, cpys, cnt);
	}

	for (size_t i = 0; i < list_length(&connections); i++) {
		struct websocket_connection *c = list_at(&connections, i);

		websocket_connection_write(c, cpys, cnt);
	}

	sample_put_many(cpys, avail);

	return cnt;
}

int websocket_parse(struct node *n, config_setting_t *cfg)
{
	struct websocket *w = n->_vd;
	config_setting_t *cfg_dests;
	int ret;

	list_init(&w->connections);
	list_init(&w->destinations);

	cfg_dests = config_setting_get_member(cfg, "destinations");
	if (cfg_dests) {
		if (!config_setting_is_array(cfg_dests))
			cerror(cfg_dests, "The 'destinations' setting must be an array of URLs");

		for (int i = 0; i < config_setting_length(cfg_dests); i++) {
			const char *uri, *prot, *ads, *path;

			uri = config_setting_get_string_elem(cfg_dests, i);
			if (!uri)
				cerror(cfg_dests, "The 'destinations' setting must be an array of URLs");

			struct websocket_destination d;

			memset(&d, 0, sizeof(d));

			d.uri = strdup(uri);
			if (!d.uri)
				serror("Failed to allocate memory");

			ret = lws_parse_uri(d.uri, &prot, &ads, &d.info.port, &path);
			if (ret)
				cerror(cfg_dests, "Failed to parse websocket URI: '%s'", uri);

			d.info.ssl_connection = !strcmp(prot, "https");
			d.info.address = strdup(ads);
			d.info.host    = d.info.address;
			d.info.origin  = d.info.address;
			d.info.ietf_version_or_minus_one = -1;
			d.info.protocol = "live";

			asprintf((char **) &d.info.path, "/%s", path);

			list_push(&w->destinations, memdup(&d, sizeof(d)));
		}
	}

	return 0;
}

char * websocket_print(struct node *n)
{
	struct websocket *w = n->_vd;

	char *buf = NULL;

	buf = strcatf(&buf, "destinations=[ ");

	for (size_t i = 0; i < list_length(&w->destinations); i++) {
		struct websocket_destination *d = list_at(&w->destinations, i);

		buf = strcatf(&buf, "%s://%s:%d%s ",
			d->info.ssl_connection ? "wss" : "ws",
			d->info.address,
			d->info.port,
			d->info.path
		);
	}

	buf = strcatf(&buf, "]");

	return buf;
}

static struct plugin p = {
	.name		= "websocket",
	.description	= "Send and receive samples of a WebSocket connection (libwebsockets)",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0, /* unlimited */
		.size		= sizeof(struct websocket),
		.init		= websocket_init,
		.deinit		= websocket_deinit,
		.start		= websocket_start,
		.stop		= websocket_stop,
		.destroy	= websocket_destroy,
		.read		= websocket_read,
		.write		= websocket_write,
		.print		= websocket_print,
		.parse		= websocket_parse,
		.instances	= LIST_INIT()
	}
};

REGISTER_PLUGIN(&p)
