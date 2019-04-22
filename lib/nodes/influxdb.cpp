/** Node-type for InfluxDB.
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
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <villas/node.h>
#include <villas/plugin.h>
#include <villas/signal.h>
#include <villas/node/config.h>
#include <villas/nodes/influxdb.hpp>
#include <villas/memory.h>

int influxdb_parse(struct node *n, json_t *json)
{
	struct influxdb *i = (struct influxdb *) n->_vd;

	json_error_t err;
	int ret;

	char *tmp, *host, *port, *lasts;
	const char *server, *key;

	ret = json_unpack_ex(json, &err, 0, "{ s: s, s: s }",
		"server", &server,
		"key", &key
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	tmp = strdup(server);

	host = strtok_r(tmp, ":", &lasts);
	port = strtok_r(nullptr, "", &lasts);

	i->key = strdup(key);
	i->host = strdup(host);
	i->port = strdup(port ? port : "8089");

	free(tmp);

	return 0;
}

int influxdb_open(struct node *n)
{
	int ret;
	struct influxdb *i = (struct influxdb *) n->_vd;

	struct addrinfo hints, *servinfo, *p;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	ret = getaddrinfo(i->host, i->port, &hints, &servinfo);
	if (ret)
	    error("Failed to lookup server: %s", gai_strerror(ret));

	/* Loop through all the results and connect to the first we can */
	for (p = servinfo; p != nullptr; p = p->ai_next) {
		i->sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (i->sd == -1) {
	        	serror("socket");
			continue;
	    	}

		ret = connect(i->sd, p->ai_addr, p->ai_addrlen);
		if (ret == -1) {
			warning("connect");
	        	close(i->sd);
	        	continue;
		}

		/* If we get here, we must have connected successfully */
		break;
	}

	return p ? 0 : -1;
}

int influxdb_close(struct node *n)
{
	struct influxdb *i = (struct influxdb *) n->_vd;

	close(i->sd);

	if (i->host)
		free(i->host);
	if (i->port)
		free(i->port);
	if (i->key)
		free(i->key);

	return 0;
}

int influxdb_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct influxdb *i = (struct influxdb *) n->_vd;

	char *buf = nullptr;
	ssize_t sentlen, buflen;

	for (unsigned k = 0; k < cnt; k++) {
		struct sample *smp = smps[k];

		/* Key */
		strcatf(&buf, "%s", i->key);

		/* Fields */
		for (unsigned j = 0; j < smp->length; j++) {
			struct signal *sig = (struct signal *) vlist_at(smp->signals, j);
			union signal_data *data = &smp->data[k];

			if (
				sig->type != SIGNAL_TYPE_BOOLEAN &&
				sig->type != SIGNAL_TYPE_INTEGER &&
				sig->type != SIGNAL_TYPE_INTEGER
			) {
				warning("Unsupported signal format for node %s. Skipping", node_name(n));
				continue;
			}

			strcatf(&buf, "%c", j == 0 ? ' ' : ',');

			sig = (struct signal *) vlist_at(smp->signals, j);
			if (!sig)
				return -1;

			if (sig->name)
				strcatf(&buf, "%s=", sig->name);
			else
				strcatf(&buf, "value%d=", j);

			switch (sig->type) {
				case SIGNAL_TYPE_BOOLEAN:
					strcatf(&buf, "%s", data->b ? "true" : "false");
					break;

				case SIGNAL_TYPE_FLOAT:
					strcatf(&buf, "%f", data->f);
					break;

				case SIGNAL_TYPE_INTEGER:
					strcatf(&buf, "%" PRIi64, data->i);
					break;

				default: { }
			}
		}

		/* Timestamp */
		strcatf(&buf, " %ld%09ld\n", smp->ts.origin.tv_sec, smp->ts.origin.tv_nsec);
	}

	buflen = strlen(buf) + 1;
	sentlen = send(i->sd, buf, buflen, 0);
	if (sentlen < 0)
		return -1;
	else if (sentlen < buflen)
		warning("Partial sent");

	free(buf);

	return cnt;
}

char * influxdb_print(struct node *n)
{
	struct influxdb *i = (struct influxdb *) n->_vd;
	char *buf = nullptr;

	strcatf(&buf, "host=%s, port=%s, key=%s", i->host, i->port, i->key);

	return buf;
}

static struct plugin p = {
	.name = "influxdb",
	.description = "Write results to InfluxDB",
	.type = PLUGIN_TYPE_NODE,
	.node = {
		.vectorize = 0,
		.size	= sizeof(struct influxdb),
		.parse	= influxdb_parse,
		.print	= influxdb_print,
		.start	= influxdb_open,
		.stop	= influxdb_close,
		.write	= influxdb_write,
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
