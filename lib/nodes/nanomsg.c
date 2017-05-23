/** Node type: nanomsg
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

#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <string.h>

#include "plugin.h"
#include "nodes/nanomsg.h"
#include "utils.h"
#include "msg.h"

int nanomsg_reverse(struct node *n)
{
	struct nanomsg *m = n->_vd;

	if (list_length(&m->publisher.endpoints)  != 1 ||
	    list_length(&m->subscriber.endpoints) != 1)
		return -1;
	
	char *subscriber = list_first(&m->subscriber.endpoints);
	char *publisher = list_first(&m->publisher.endpoints);
	
	list_set(&m->subscriber.endpoints, 0, publisher);
	list_set(&m->publisher.endpoints, 0, subscriber);

	return 0;
}

static int nanomsg_parse_endpoints(struct list *l, config_setting_t *cfg)
{
	const char *ep;

	switch (config_setting_type(cfg)) {
		case CONFIG_TYPE_LIST:
		case CONFIG_TYPE_ARRAY:
			for (int j = 0; j < config_setting_length(cfg); j++) {
				const char *ep = config_setting_get_string_elem(cfg, j);
				
				list_push(l, strdup(ep));
			}
			break;

		case CONFIG_TYPE_STRING:
			ep = config_setting_get_string(cfg);
			
			list_push(l, strdup(ep));
			
			break;

		default:
			return -1;
	}
	
	return 0;
}

int nanomsg_parse(struct node *n, config_setting_t *cfg)
{
	int ret;
	struct nanomsg *m = n->_vd;

	config_setting_t *cfg_pub, *cfg_sub;

	list_init(&m->publisher.endpoints);
	list_init(&m->subscriber.endpoints);

	cfg_pub = config_setting_lookup(cfg, "publish");
	if (cfg_pub) {
		ret = nanomsg_parse_endpoints(&m->publisher.endpoints, cfg_pub);
		if (ret < 0)
			cerror(cfg_pub, "Invalid type for 'publish' setting of node %s", node_name(n));
	}

	cfg_sub = config_setting_lookup(cfg, "subscribe");
	if (cfg_sub) {
		ret = nanomsg_parse_endpoints(&m->subscriber.endpoints, cfg_sub);
		if (ret < 0)
			cerror(cfg_pub, "Invalid type for 'subscribe' setting of node %s", node_name(n));
	}

	return 0;
}

char * nanomsg_print(struct node *n)
{
	struct nanomsg *m = n->_vd;
	
	char *buf = NULL;

	strcatf(&buf, "subscribe=[ ");
	
	for (size_t i = 0; i < list_length(&m->subscriber.endpoints); i++) {
		char *ep = list_at(&m->subscriber.endpoints, i);
		
		strcatf(&buf, "%s ", ep);
	}
	
	strcatf(&buf, "], publish=[ ");
	
	for (size_t i = 0; i < list_length(&m->publisher.endpoints); i++) {
		char *ep = list_at(&m->publisher.endpoints, i);
		
		strcatf(&buf, "%s ", ep);
	}

	strcatf(&buf, " ]");

	return buf;
}

int nanomsg_start(struct node *n)
{
	int ret;
	struct nanomsg *m = n->_vd;
	
	ret = m->subscriber.socket = nn_socket(AF_SP, NN_SUB);
	if (ret < 0)
		return ret;
	
	ret = m->publisher.socket = nn_socket(AF_SP, NN_PUB);
	if (ret < 0)
		return ret;
	
	/* Subscribe to all topics */
	ret = nn_setsockopt(ret = m->subscriber.socket, NN_SUB, NN_SUB_SUBSCRIBE, "", 0);
	if (ret < 0)
		return ret;
	
	/* Bind publisher to socket */
	for (size_t i = 0; i < list_length(&m->publisher.endpoints); i++) {
		char *ep = list_at(&m->publisher.endpoints, i);
		
		ret = nn_bind(m->publisher.socket, ep);
		if (ret < 0) {
			info("Failed to connect nanomsg socket: node=%s, endpoint=%s, error=%s", node_name(n), ep, nn_strerror(errno));
			return ret;
		}
	}
	
	/* Sonnect subscribers socket */
	for (size_t i = 0; i < list_length(&m->subscriber.endpoints); i++) {
		char *ep = list_at(&m->subscriber.endpoints, i);
		
		ret = nn_connect(m->subscriber.socket, ep);
		if (ret < 0) {
			info("Failed to connect nanomsg socket: node=%s, endpoint=%s, error=%s", node_name(n), ep, nn_strerror(errno));
			return ret;
		}
	}

	return 0;
}

int nanomsg_stop(struct node *n)
{
	int ret;
	struct nanomsg *m = n->_vd;

	ret = nn_shutdown(m->subscriber.socket, 0);
	if (ret < 0)
		return ret;
	
	ret = nn_shutdown(m->publisher.socket, 0);
	if (ret < 0)
		return ret;

	return 0;
}

int nanomsg_deinit()
{
	nn_term();
	
	return 0;
}

int nanomsg_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct nanomsg *m = n->_vd;
	
	char data[MSG_MAX_PACKET_LEN];

	/* Receive payload */
	ret = nn_recv(m->subscriber.socket, data, sizeof(data), 0);
	if (ret < 0)
		return ret;

	return msg_buffer_to_samples(smps, cnt, data, ret);
}

int nanomsg_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct nanomsg *m = n->_vd;

	ssize_t sent;
	
	char data[MSG_MAX_PACKET_LEN];

	sent = msg_buffer_from_samples(smps, cnt, data, sizeof(data));
	if (sent < 0)
		return -1;

	ret = nn_send(m->publisher.socket, data, sent, 0);
	if (ret < 0)
		return ret;

	return cnt;
}

static struct plugin p = {
	.name		= "nanomsg",
	.description	= "scalability protocols library",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct nanomsg),
		.reverse	= nanomsg_reverse,
		.parse		= nanomsg_parse,
		.print		= nanomsg_print,
		.start		= nanomsg_start,
		.stop		= nanomsg_stop,
		.deinit		= nanomsg_deinit,
		.read		= nanomsg_read,
		.write		= nanomsg_write,
		.instances	= LIST_INIT()
	}
};

REGISTER_PLUGIN(&p)