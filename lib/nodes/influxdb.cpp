/** Node-type for InfluxDB.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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
#include <cinttypes>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <villas/signal.hpp>
#include <villas/sample.hpp>
#include <villas/node/config.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/influxdb.hpp>
#include <villas/node/memory.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int villas::node::influxdb_parse(NodeCompat *n, json_t *json)
{
	auto *i = n->getData<struct influxdb>();

	json_error_t err;
	int ret;

	char *tmp, *host, *port, *lasts;
	const char *server, *key;

	ret = json_unpack_ex(json, &err, 0, "{ s: s, s: s }",
		"server", &server,
		"key", &key
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-influx");

	tmp = strdup(server);

	host = strtok_r(tmp, ":", &lasts);
	port = strtok_r(nullptr, "", &lasts);

	i->key = strdup(key);
	i->host = strdup(host);
	i->port = strdup(port ? port : "8089");

	free(tmp);

	return 0;
}

int villas::node::influxdb_open(NodeCompat *n)
{
	int ret;
	auto *i = n->getData<struct influxdb>();

	struct addrinfo hints, *servinfo, *p;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	ret = getaddrinfo(i->host, i->port, &hints, &servinfo);
	if (ret)
		throw RuntimeError("Failed to lookup server: {}", gai_strerror(ret));

	/* Loop through all the results and connect to the first we can */
	for (p = servinfo; p != nullptr; p = p->ai_next) {
		i->sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (i->sd == -1)
			throw SystemError("Failed to create socket");

		ret = connect(i->sd, p->ai_addr, p->ai_addrlen);
		if (ret == -1) {
			n->logger->warn("Connect failed: {}", strerror(errno));
			close(i->sd);
			continue;
		}

		/* If we get here, we must have connected successfully */
		break;
	}

	return p ? 0 : -1;
}

int villas::node::influxdb_close(NodeCompat *n)
{
	auto *i = n->getData<struct influxdb>();

	close(i->sd);

	if (i->host)
		free(i->host);
	if (i->port)
		free(i->port);
	if (i->key)
		free(i->key);

	return 0;
}

int villas::node::influxdb_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	auto *i = n->getData<struct influxdb>();

	char *buf = strf("");
	ssize_t sentlen, buflen;

	for (unsigned k = 0; k < cnt; k++) {
		const struct Sample *smp = smps[k];

		/* Key */
		strcatf(&buf, "%s", i->key);

		/* Fields */
		for (unsigned j = 0; j < smp->length; j++) {
			const auto *data = &smp->data[j];
			auto sig = smp->signals->getByIndex(j);
			if (!sig)
				return -1;

			if (
				sig->type != SignalType::BOOLEAN &&
				sig->type != SignalType::FLOAT &&
				sig->type != SignalType::INTEGER &&
				sig->type != SignalType::COMPLEX
			) {
				n->logger->warn("Unsupported signal format. Skipping");
				continue;
			}

			strcatf(&buf, "%c", j == 0 ? ' ' : ',');
			if (sig->type == SignalType::COMPLEX) {
				strcatf(&buf, "%s_re=%f, %s_im=%f",
					sig->name.c_str(), std::real(data->z),
					sig->name.c_str(), std::imag(data->z)
				);
			}
			else {
				strcatf(&buf, "%s=", sig->name.c_str());

				switch (sig->type) {
					case SignalType::BOOLEAN:
						strcatf(&buf, "%s", data->b ? "true" : "false");
						break;

					case SignalType::FLOAT:
						strcatf(&buf, "%f", data->f);
						break;

					case SignalType::INTEGER:
						strcatf(&buf, "%" PRIi64, data->i);
						break;

					default: { }
				}
			}
		}

		/* Timestamp */
		strcatf(&buf, " %lld%09lld\n", (long long) smp->ts.origin.tv_sec,
					       (long long) smp->ts.origin.tv_nsec);
	}

	buflen = strlen(buf) + 1;
	sentlen = send(i->sd, buf, buflen, 0);
	if (sentlen < 0)
		return -1;
	else if (sentlen < buflen)
		n->logger->warn("Partial sent");

	free(buf);

	return cnt;
}

char * villas::node::influxdb_print(NodeCompat *n)
{
	auto *i = n->getData<struct influxdb>();
	char *buf = nullptr;

	strcatf(&buf, "host=%s, port=%s, key=%s", i->host, i->port, i->key);

	return buf;
}

static NodeCompatType p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "influxdb";
	p.description	= "Write results to InfluxDB";
	p.vectorize	= 0;
	p.size		= sizeof(struct influxdb);
	p.parse		= influxdb_parse;
	p.print		= influxdb_print;
	p.start		= influxdb_open;
	p.stop		= influxdb_close;
	p.write		= influxdb_write;

	static NodeCompatFactory ncp(&p);
}
