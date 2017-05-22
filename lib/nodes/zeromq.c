/** Node type: ZeroMQ
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

#include <zmq.h>

#include "nodes/zeromq.h"
#include "utils.h"
#include "queue.h"
#include "plugin.h"

static void *context;

/* Release our samples. */
static void free_msg(void *data, void *hint)
{
	struct sample *s = data;
	
	sample_put(s);
}

int zeromq_reverse(struct node *n)
{
//	struct zeromq *z = n->_vd;

	return 0;
}

int zeromq_parse(struct node *n, config_setting_t *cfg)
{
	struct zeromq *z = n->_vd;
	
	const char *ep, *type;
	
	config_setting_t *cfg_pub;
	
	list_init(&z->publisher.endpoints);
	
	if (config_setting_lookup_string(cfg, "subscribe", &ep))
		z->subscriber.endpoint = strdup(ep);
	else
		z->subscriber.endpoint = NULL;
	
	cfg_pub = config_setting_lookup(cfg, "publish");
	if (cfg_pub) {
		switch (config_setting_type(cfg_pub)) {
			case CONFIG_TYPE_LIST:
			case CONFIG_TYPE_ARRAY:
				for (int j = 0; j < config_setting_length(cfg_pub); j++) {
					const char *ep = config_setting_get_string_elem(cfg_pub, j);
					
					list_push(&z->publisher.endpoints, strdup(ep));
				}
				break;

			case CONFIG_TYPE_STRING:
				ep = config_setting_get_string(cfg_pub);
				
				list_push(&z->publisher.endpoints, strdup(ep));
				
				break;

			default:
				cerror(cfg_pub, "Invalid type for ZeroMQ publisher setting");
		}
	}
	
	if (config_setting_lookup_string(cfg, "pattern", &type)) {
		if      (!strcmp(type, "pubsub"))
			z->pattern = ZEROMQ_PATTERN_PUBSUB;
		else if (!strcmp(type, "radiodish"))
			z->pattern = ZEROMQ_PATTERN_RADIODISH;
		else
			cerror(cfg, "Invalid type for ZeroMQ node: %s", node_name_short(n));
	}

	return 0;
}

char * zeromq_print(struct node *n)
{
	struct zeromq *z = n->_vd;

	char *buf = NULL;
	char *pattern = NULL;

	switch (z->pattern) {
		case ZEROMQ_PATTERN_PUBSUB: pattern = "pubsub"; break;
		case ZEROMQ_PATTERN_RADIODISH: pattern = "radiodish"; break;
	}
	
	strcatf(&buf, "pattern=%s, subscribe=%s, publish=[ ", pattern, z->subscriber.endpoint);
	
	for (size_t i = 0; i < list_length(&z->publisher.endpoints); i++) {
		char *ep = list_at(&z->publisher.endpoints, i);
		
		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, " ]");

	return buf;
}

int zeromq_init(struct super_node *sn)
{
	context = zmq_ctx_new();
	
	info("context is %p", context);
	
	return context == NULL;
}

int zeromq_deinit()
{
	return zmq_ctx_term(context);
}

int zeromq_start(struct node *n)
{
	int ret;
	struct zeromq *z = n->_vd;
	
	switch (z->pattern) {
		case ZEROMQ_PATTERN_RADIODISH:
			z->subscriber.socket = zmq_socket(context, ZMQ_RADIO);
			z->publisher.socket  = zmq_socket(context, ZMQ_DISH);
			break;
		
		case ZEROMQ_PATTERN_PUBSUB:
			z->subscriber.socket = zmq_socket(context, ZMQ_SUB);
			z->publisher.socket  = zmq_socket(context, ZMQ_PUB);
			break;
	}

	/* Bind subscriber socket */
	if (z->subscriber.endpoint) {
		ret = zmq_bind(z->subscriber.socket, z->subscriber.endpoint);
		if (ret)
			return ret;
	}
	
	/* Subscribe to all pubsub messages. */
	zmq_setsockopt(z->subscriber.socket, ZMQ_SUBSCRIBE, NULL, 0);
	
	/* Connect publisher socket */
	for (size_t i = 0; i < list_length(&z->publisher.endpoints); i++) {
		char *ep = list_at(&z->publisher.endpoints, i);
		
		ret = zmq_connect(z->publisher.socket, ep);
		if (ret)
			return ret;
	}

	return 0;
}

int zeromq_stop(struct node *n)
{
	int ret;
	struct zeromq *z = n->_vd;
	
	ret = zmq_close(z->subscriber.socket);
	if (ret)
		return ret;

	return zmq_close(z->publisher.socket);
}

int zeromq_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int i, ret;
	struct zeromq *z = n->_vd;

	for (i = 0; i < cnt; i++) {
		zmq_msg_t m;
		
		ret = zmq_msg_init_data(&m, smps[i], SAMPLE_LEN(smps[i]->capacity), free_msg, NULL);
		if (ret < 0)
			break;
		
		ret = zmq_msg_recv(&m, z->subscriber.socket, 0);
		if (ret < 0)
			break;
	}

	return i;
}

int zeromq_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int i, ret;
	struct zeromq *z = n->_vd;

	for (i = 0; i < cnt; i++) {
		zmq_msg_t m;
		
		sample_get(smps[i]);
		
		ret = zmq_msg_init_data(&m, smps[i], SAMPLE_LEN(smps[i]->length), free_msg, NULL);
		if (ret < 0)
			break;
		
		ret = zmq_msg_send(&m, z->publisher.socket, 0);
		if (ret < 0)
			break;
	}

	return i;
}

static struct plugin p = {
	.name		= "zeromq",
	.description	= "ZeroMQ Distributed Messaging",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct zeromq),
		.reverse	= zeromq_reverse,
		.parse		= zeromq_parse,
		.print		= zeromq_print,
		.start		= zeromq_start,
		.stop		= zeromq_stop,
		.init		= zeromq_init,
		.deinit		= zeromq_deinit,
		.read		= zeromq_read,
		.write		= zeromq_write,
		.instances	= LIST_INIT()
	}
};

REGISTER_PLUGIN(&p)