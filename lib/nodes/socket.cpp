/** The socket node-type for Layer 2, 3, 4 BSD-style sockets
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

#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <arpa/inet.h>
#include <netinet/ip.h>

#include <villas/node_compat.hpp>
#include <villas/nodes/socket.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/queue.h>
#include <villas/compat.hpp>
#include <villas/super_node.hpp>

#ifdef WITH_SOCKET_LAYER_ETH
  #include <netinet/ether.h>
#endif /* WITH_SOCKET_LAYER_ETH */

#ifdef WITH_NETEM
  #include <villas/kernel/if.hpp>
  #include <villas/kernel/nl.hpp>
#endif /* WITH_NETEM */

using namespace villas;
using namespace villas::utils;
using namespace villas::node;
using namespace villas::kernel;

/* Forward declartions */
static NodeCompatType p;
static NodeCompatFactory ncp(&p);

int villas::node::socket_type_start(villas::node::SuperNode *sn)
{
#ifdef WITH_NETEM
	/* Gather list of used network interfaces */
	for (auto *n : ncp.instances) {
		auto *nc = dynamic_cast<NodeCompat *>(n);
		auto *s = nc->getData<struct Socket>();

		if (s->layer == SocketLayer::UNIX)
			continue;

		/* Determine outgoing interface */
		Interface *j = Interface::getEgress((struct sockaddr *) &s->out.saddr, sn);

		j->addNode(n);
	}
#endif /* WITH_NETEM */

	return 0;
}

int villas::node::socket_init(NodeCompat *n)
{
	auto *s = n->getData<struct Socket>();

	s->formatter = nullptr;

	return 0;
}

int villas::node::socket_destroy(NodeCompat *n)
{
	auto *s = n->getData<struct Socket>();

	if (s->formatter)
		delete s->formatter;

	return 0;
}

char * villas::node::socket_print(NodeCompat *n)
{
	auto *s = n->getData<struct Socket>();
	const char *layer = nullptr;
	char *buf;

	switch (s->layer) {
		case SocketLayer::UDP:
			layer = "udp";
			break;

		case SocketLayer::IP:
			layer = "ip";
			break;

		case SocketLayer::ETH:
			layer = "eth";
			break;

		case SocketLayer::UNIX:
			layer = "unix";
			break;
	}

	char *local = socket_print_addr((struct sockaddr *) &s->in.saddr);
	char *remote = socket_print_addr((struct sockaddr *) &s->out.saddr);

	buf = strf("layer=%s, in.address=%s, out.address=%s", layer, local, remote);

	if (s->multicast.enabled) {
		char group[INET_ADDRSTRLEN];
		char interface[INET_ADDRSTRLEN];

		inet_ntop(AF_INET, &s->multicast.mreq.imr_multiaddr, group, sizeof(group));
		inet_ntop(AF_INET, &s->multicast.mreq.imr_interface, interface, sizeof(interface));

		strcatf(&buf, ", in.multicast.enabled=%s", s->multicast.enabled ? "yes" : "no");
		strcatf(&buf, ", in.multicast.loop=%s",    s->multicast.loop    ? "yes" : "no");
		strcatf(&buf, ", in.multicast.group=%s", group);
		strcatf(&buf, ", in.multicast.interface=%s", s->multicast.mreq.imr_interface.s_addr == INADDR_ANY ? "any" : interface);
		strcatf(&buf, ", in.multicast.ttl=%u", s->multicast.ttl);
	}

	free(local);
	free(remote);

	return buf;
}

int villas::node::socket_check(NodeCompat *n)
{
	auto *s = n->getData<struct Socket>();

	/* Some checks on the addresses */
	if (s->layer != SocketLayer::UNIX) {
		if (s->in.saddr.sa.sa_family != s->out.saddr.sa.sa_family)
			throw RuntimeError("Address families of local and remote must match!");
	}

	if (s->layer == SocketLayer::IP) {
		if (ntohs(s->in.saddr.sin.sin_port) != ntohs(s->out.saddr.sin.sin_port))
			throw RuntimeError("IP protocol numbers of local and remote must match!");
	}
#ifdef WITH_SOCKET_LAYER_ETH
	else if (s->layer == SocketLayer::ETH) {
		if (ntohs(s->in.saddr.sll.sll_protocol) != ntohs(s->out.saddr.sll.sll_protocol))
			throw RuntimeError("Ethertypes of local and remote must match!");

		if (ntohs(s->in.saddr.sll.sll_protocol) <= 0x5DC)
			throw RuntimeError("Ethertype must be large than {} or it is interpreted as an IEEE802.3 length field!", 0x5DC);
	}
#endif /* WITH_SOCKET_LAYER_ETH */

	if (s->multicast.enabled) {
		if (s->in.saddr.sa.sa_family != AF_INET)
			throw RuntimeError("Multicast is only supported by IPv4");

		uint32_t addr = ntohl(s->multicast.mreq.imr_multiaddr.s_addr);
		if ((addr >> 28) != 14)
			throw RuntimeError("Multicast group address must be within 224.0.0.0/4");
	}

	return 0;
}

int villas::node::socket_start(NodeCompat *n)
{
	auto *s = n->getData<struct Socket>();
	int ret;

	/* Initialize IO */
	s->formatter->start(n->getInputSignals(false), ~(int) SampleFlags::HAS_OFFSET);

	/* Create socket */
	switch (s->layer) {
		case SocketLayer::UDP:
			s->sd = socket(s->in.saddr.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP);
			break;

		case SocketLayer::IP:
			s->sd = socket(s->in.saddr.sa.sa_family, SOCK_RAW, ntohs(s->in.saddr.sin.sin_port));
			break;

#ifdef WITH_SOCKET_LAYER_ETH
		case SocketLayer::ETH:
			s->sd = socket(s->in.saddr.sa.sa_family, SOCK_DGRAM, s->in.saddr.sll.sll_protocol);
			break;
#endif /* WITH_SOCKET_LAYER_ETH */

		case SocketLayer::UNIX:
			s->sd = socket(s->in.saddr.sa.sa_family, SOCK_DGRAM, 0);
			break;

		default:
			throw RuntimeError("Invalid socket type!");
	}

	if (s->sd < 0)
		throw SystemError("Failed to create socket");

	/* Delete Unix domain socket if already existing */
	if (s->layer == SocketLayer::UNIX) {
		ret = unlink(s->in.saddr.sun.sun_path);
		if (ret && errno != ENOENT)
			return ret;
	}

	/* Bind socket for receiving */
	socklen_t addrlen = 0;
	switch(s->in.saddr.ss.ss_family) {
		case AF_INET:
			addrlen = sizeof(struct sockaddr_in);
			break;

		case AF_INET6:
			addrlen = sizeof(struct sockaddr_in6);
			break;

		case AF_UNIX:
			addrlen = SUN_LEN(&s->in.saddr.sun);
			break;

#ifdef WITH_SOCKET_LAYER_ETH
		case AF_PACKET:
			addrlen = sizeof(struct sockaddr_ll);
			break;
#endif /* WITH_SOCKET_LAYER_ETH */
		default:
			addrlen = sizeof(s->in.saddr);
	}

	ret = bind(s->sd, (struct sockaddr *) &s->in.saddr, addrlen);
	if (ret < 0)
		throw SystemError("Failed to bind socket");

	if (s->multicast.enabled) {
		ret = setsockopt(s->sd, IPPROTO_IP, IP_MULTICAST_LOOP, &s->multicast.loop, sizeof(s->multicast.loop));
		if (ret)
			throw SystemError("Failed to set multicast loop option");

		ret = setsockopt(s->sd, IPPROTO_IP, IP_MULTICAST_TTL, &s->multicast.ttl, sizeof(s->multicast.ttl));
		if (ret)
			throw SystemError("Failed to set multicast ttl option");

		ret = setsockopt(s->sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &s->multicast.mreq, sizeof(s->multicast.mreq));
		if (ret)
			throw SystemError("Failed to join multicast group");
	}

	/* Set socket priority, QoS or TOS IP options */
	int prio;
	switch (s->layer) {
		case SocketLayer::UDP:
		case SocketLayer::IP:
			prio = IPTOS_LOWDELAY;
			if (setsockopt(s->sd, IPPROTO_IP, IP_TOS, &prio, sizeof(prio)))
				throw SystemError("Failed to set type of service (QoS)");
			else
				n->logger->debug("Set QoS/TOS IP option to {:#x}", prio);
			break;

		default:
#ifdef __linux__
			prio = SOCKET_PRIO;
			if (setsockopt(s->sd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
				throw SystemError("Failed to set socket priority");
			else
				n->logger->debug("Set socket priority to {}", prio);
			break;
#else
			{ }
#endif /* __linux__ */
	}

	s->out.buflen = SOCKET_INITIAL_BUFFER_LEN;
	s->out.buf = new char[s->out.buflen];
	if (!s->out.buf)
		throw MemoryAllocationError();

	s->in.buflen = SOCKET_INITIAL_BUFFER_LEN;
	s->in.buf = new char[s->in.buflen];
	if (!s->in.buf)
		throw MemoryAllocationError();

	return 0;
}

int villas::node::socket_reverse(NodeCompat *n)
{
	auto *s = n->getData<struct Socket>();
	union sockaddr_union tmp;

	tmp = s->in.saddr;
	s->in.saddr = s->out.saddr;
	s->out.saddr = tmp;

	return 0;
}

int villas::node::socket_stop(NodeCompat *n)
{
	int ret;
	auto *s = n->getData<struct Socket>();

	if (s->multicast.enabled) {
		ret = setsockopt(s->sd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &s->multicast.mreq, sizeof(s->multicast.mreq));
		if (ret)
			throw SystemError("Failed to leave multicast group");
	}

	if (s->sd >= 0) {
		ret = close(s->sd);
		if (ret)
			return ret;
	}

	delete[] s->in.buf;
	delete[] s->out.buf;

	return 0;
}

int villas::node::socket_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int ret;
	auto *s = n->getData<struct Socket>();

	char *ptr;
	ssize_t bytes;
	size_t rbytes;

	union sockaddr_union src;
	socklen_t srclen = sizeof(src);

	/* Receive next sample */
	bytes = recvfrom(s->sd, s->in.buf, s->in.buflen, 0, &src.sa, &srclen);
	if (bytes < 0) {
		if (errno == EINTR)
			return -1;

		throw SystemError("Failed recvfrom()");
	}
	else if (bytes == 0)
		return 0;

	ptr = s->in.buf;

	/* Strip IP header from packet */
	if (s->layer == SocketLayer::IP) {
		struct ip *iphdr = (struct ip *) ptr;

		bytes -= iphdr->ip_hl * 4;
		ptr += iphdr->ip_hl * 4;
	}

	/* SOCK_RAW IP sockets to not provide the IP protocol number via recvmsg()
	 * So we simply set it ourself. */
	if (s->layer == SocketLayer::IP) {
		switch (src.sa.sa_family) {
			case AF_INET:
				src.sin.sin_port = s->out.saddr.sin.sin_port;
				break;

			case AF_INET6:
				src.sin6.sin6_port = s->out.saddr.sin6.sin6_port;
				break;
		}
	}

	if (s->verify_source && socket_compare_addr(&src.sa, &s->out.saddr.sa) != 0) {
		char *buf = socket_print_addr((struct sockaddr *) &src);
		n->logger->warn("Received packet from unauthorized source: {}", buf);
		free(buf);

		return 0;
	}

	ret = s->formatter->sscan(ptr, bytes, &rbytes, smps, cnt);
	if (ret < 0 || (size_t) bytes != rbytes)
		n->logger->warn("Received invalid packet: ret={}, bytes={}, rbytes={}", ret, bytes, rbytes);

	return ret;
}

int villas::node::socket_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	auto *s = n->getData<struct Socket>();

	int ret;
	ssize_t bytes;
	size_t wbytes;

retry:	ret = s->formatter->sprint(s->out.buf, s->out.buflen, &wbytes, smps, cnt);
	if (ret < 0) {
		n->logger->warn("Failed to format payload: reason={}", ret);
		return ret;
	}

	if (wbytes == 0) {
		n->logger->warn("Failed to format payload: wbytes={}", wbytes);
		return -1;
	}

	if (wbytes > s->out.buflen) {
		s->out.buflen = wbytes;

		delete[] s->out.buf;
		s->out.buf = new char[s->out.buflen];
		if (!s->out.buf)
			throw MemoryAllocationError();

		goto retry;
	}

	/* Send message */
	socklen_t addrlen = 0;
	switch(s->in.saddr.ss.ss_family) {
		case AF_INET:
			addrlen = sizeof(struct sockaddr_in);
			break;

		case AF_INET6:
			addrlen = sizeof(struct sockaddr_in6);
			break;

		case AF_UNIX:
			addrlen = SUN_LEN(&s->out.saddr.sun);
			break;

#ifdef WITH_SOCKET_LAYER_ETH
		case AF_PACKET:
			addrlen = sizeof(struct sockaddr_ll);
			break;
#endif /* WITH_SOCKET_LAYER_ETH */
		default:
			addrlen = sizeof(s->in.saddr);
	}

retry2:	bytes = sendto(s->sd, s->out.buf, wbytes, 0, (struct sockaddr *) &s->out.saddr, addrlen);
	if (bytes < 0) {
		if ((errno == EPERM) ||
		    (errno == ENOENT && s->layer == SocketLayer::UNIX))
			n->logger->warn("Failed sendto(): {}", strerror(errno));
		else if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			n->logger->warn("Blocking sendto()");
			goto retry2;
		}
		else
			n->logger->warn("Failed sendto(): {}", strerror(errno));
	}
	else if ((size_t) bytes < wbytes)
		n->logger->warn("Partial sendto()");

	return cnt;
}

int villas::node::socket_parse(NodeCompat *n, json_t *json)
{
	int ret;
	auto *s = n->getData<struct Socket>();

	const char *local, *remote;
	const char *layer = nullptr;

	json_error_t err;
	json_t *json_multicast = nullptr;
	json_t *json_format = nullptr;

	/* Default values */
	s->layer = SocketLayer::UDP;
	s->verify_source = 0;

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: o, s: { s: s }, s: { s: s, s?: b, s?: o } }",
		"layer", &layer,
		"format", &json_format,
		"out",
			"address", &remote,
		"in",
			"address", &local,
			"verify_source", &s->verify_source,
			"multicast", &json_multicast
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-socket");

	/* Format */
	if (s->formatter)
		delete s->formatter;
	s->formatter = json_format
			? FormatFactory::make(json_format)
			: FormatFactory::make("villas.binary");
	if (!s->formatter)
		throw ConfigError(json_format, "node-config-node-socket-format", "Invalid format configuration");

	/* IP layer */
	if (layer) {
		if (!strcmp(layer, "ip"))
			s->layer = SocketLayer::IP;
#ifdef WITH_SOCKET_LAYER_ETH
		else if (!strcmp(layer, "eth"))
			s->layer = SocketLayer::ETH;
#endif /* WITH_SOCKET_LAYER_ETH */
		else if (!strcmp(layer, "udp"))
			s->layer = SocketLayer::UDP;
		else if (!strcmp(layer, "unix") || !strcmp(layer, "local"))
			s->layer = SocketLayer::UNIX;
		else
			throw SystemError("Invalid layer '{}'", layer);
	}

	ret = socket_parse_address(remote, (struct sockaddr *) &s->out.saddr, s->layer, 0);
	if (ret)
		throw SystemError("Failed to resolve remote address '{}': {}", remote, gai_strerror(ret));

	ret = socket_parse_address(local, (struct sockaddr *) &s->in.saddr, s->layer, AI_PASSIVE);
	if (ret)
		throw SystemError("Failed to resolve local address '{}': {}", local, gai_strerror(ret));

	if (json_multicast) {
		const char *group, *interface = nullptr;

		/* Default values */
		s->multicast.enabled = true;
		s->multicast.mreq.imr_interface.s_addr = INADDR_ANY;
		s->multicast.loop = 0;
		s->multicast.ttl = 255;

		ret = json_unpack_ex(json_multicast, &err, 0, "{ s?: b, s: s, s?: s, s?: b, s?: i }",
			"enabled", &s->multicast.enabled,
			"group", &group,
			"interface", &interface,
			"loop", &s->multicast.loop,
			"ttl", &s->multicast.ttl
		);
		if (ret)
			throw ConfigError(json_multicast, err, "node-config-node-socket-multicast", "Failed to parse multicast settings");

		ret = inet_aton(group, &s->multicast.mreq.imr_multiaddr);
		if (!ret)
			throw SystemError("Failed to resolve multicast group address '{}'", group);

		if (interface) {
			ret = inet_aton(group, &s->multicast.mreq.imr_interface);
			if (!ret)
				throw SystemError("Failed to resolve multicast interface address '{}'", interface);
		}
	}

	return 0;
}

int villas::node::socket_fds(NodeCompat *n, int fds[])
{
	auto *s = n->getData<struct Socket>();

	fds[0] = s->sd;

	return 1;
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "socket";
#ifdef WITH_NETEM
	p.description	= "BSD network sockets for Ethernet / IP / UDP (libnl3, netem support)";
#else
	p.description	= "BSD network sockets for Ethernet / IP / UDP";
#endif
	p.vectorize	= 0;
	p.size		= sizeof(struct Socket);
	p.type.start	= socket_type_start;
	p.reverse	= socket_reverse;
	p.init		= socket_init;
	p.destroy	= socket_destroy;
	p.parse		= socket_parse;
	p.print		= socket_print;
	p.check		= socket_check;
	p.start		= socket_start;
	p.stop		= socket_stop;
	p.read		= socket_read;
	p.write		= socket_write;
	p.poll_fds	= socket_fds;
	p.netem_fds	= socket_fds;
}
