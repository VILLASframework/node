/** The socket node-type for Layer 2, 3, 4 BSD-style sockets
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

#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <arpa/inet.h>
#include <netinet/ip.h>

#include <villas/nodes/socket.hpp>
#include <villas/utils.hpp>
#include <villas/format_type.h>
#include <villas/sample.h>
#include <villas/queue.h>
#include <villas/plugin.h>
#include <villas/compat.hpp>
#include <villas/super_node.hpp>

#ifdef WITH_SOCKET_LAYER_ETH
  #include <netinet/ether.h>
#endif /* WITH_SOCKET_LAYER_ETH */

#ifdef WITH_NETEM
  #include <villas/kernel/if.h>
  #include <villas/kernel/nl.h>
#endif /* WITH_NETEM */

/* Forward declartions */
static struct plugin p;

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int socket_type_start(villas::node::SuperNode *sn)
{
#ifdef WITH_NETEM
	struct vlist *interfaces = sn->getInterfaces();

	/* Gather list of used network interfaces */
	for (size_t i = 0; i < vlist_length(&p.node.instances); i++) {
		struct vnode *n = (struct vnode *) vlist_at(&p.node.instances, i);
		struct socket *s = (struct socket *) n->_vd;

		if (s->layer == SocketLayer::UNIX)
			continue;

		/* Determine outgoing interface */
		struct interface *j = if_get_egress((struct sockaddr *) &s->out.saddr, interfaces);

		vlist_push(&j->nodes, n);
	}
#endif /* WITH_NETEM */

	return 0;
}

char * socket_print(struct vnode *n)
{
	struct socket *s = (struct socket *) n->_vd;
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

	buf = strf("layer=%s, format=%s, in.address=%s, out.address=%s", layer, format_type_name(s->format), local, remote);

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

int socket_check(struct vnode *n)
{
	struct socket *s = (struct socket *) n->_vd;

	/* Some checks on the addresses */
	if (s->layer != SocketLayer::UNIX) {
		if (s->in.saddr.sa.sa_family != s->out.saddr.sa.sa_family)
			error("Address families of local and remote must match!");
	}

	if (s->layer == SocketLayer::IP) {
		if (ntohs(s->in.saddr.sin.sin_port) != ntohs(s->out.saddr.sin.sin_port))
			error("IP protocol numbers of local and remote must match!");
	}
#ifdef WITH_SOCKET_LAYER_ETH
	else if (s->layer == SocketLayer::ETH) {
		if (ntohs(s->in.saddr.sll.sll_protocol) != ntohs(s->out.saddr.sll.sll_protocol))
			error("Ethertypes of local and remote must match!");

		if (ntohs(s->in.saddr.sll.sll_protocol) <= 0x5DC)
			error("Ethertype must be large than %d or it is interpreted as an IEEE802.3 length field!", 0x5DC);
	}
#endif /* WITH_SOCKET_LAYER_ETH */

	if (s->multicast.enabled) {
		if (s->in.saddr.sa.sa_family != AF_INET)
			error("Multicast is only supported by IPv4 for node %s", node_name(n));

		uint32_t addr = ntohl(s->multicast.mreq.imr_multiaddr.s_addr);
		if ((addr >> 28) != 14)
			error("Multicast group address of node %s must be within 224.0.0.0/4", node_name(n));
	}

	return 0;
}

int socket_start(struct vnode *n)
{
	struct socket *s = (struct socket *) n->_vd;
	int ret;

	/* Initialize IO */
	ret = io_init(&s->io, s->format, &n->in.signals, (int) SampleFlags::HAS_ALL & ~(int) SampleFlags::HAS_OFFSET);
	if (ret)
		return ret;

	ret = io_check(&s->io);
	if (ret)
		return ret;

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
			error("Invalid socket type!");
	}

	if (s->sd < 0)
		serror("Failed to create socket");

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
		serror("Failed to bind socket");

	if (s->multicast.enabled) {
		ret = setsockopt(s->sd, IPPROTO_IP, IP_MULTICAST_LOOP, &s->multicast.loop, sizeof(s->multicast.loop));
		if (ret)
			serror("Failed to set multicast loop option");

		ret = setsockopt(s->sd, IPPROTO_IP, IP_MULTICAST_TTL, &s->multicast.ttl, sizeof(s->multicast.ttl));
		if (ret)
			serror("Failed to set multicast ttl option");

		ret = setsockopt(s->sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &s->multicast.mreq, sizeof(s->multicast.mreq));
		if (ret)
			serror("Failed to join multicast group");
	}

	/* Set socket priority, QoS or TOS IP options */
	int prio;
	switch (s->layer) {
		case SocketLayer::UDP:
		case SocketLayer::IP:
			prio = IPTOS_LOWDELAY;
			if (setsockopt(s->sd, IPPROTO_IP, IP_TOS, &prio, sizeof(prio)))
				serror("Failed to set type of service (QoS)");
			else
				debug(LOG_SOCKET | 4, "Set QoS/TOS IP option for node %s to %#x", node_name(n), prio);
			break;

		default:
#ifdef __linux__
			prio = SOCKET_PRIO;
			if (setsockopt(s->sd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
				serror("Failed to set socket priority");
			else
				debug(LOG_SOCKET | 4, "Set socket priority for node %s to %d", node_name(n), prio);
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

int socket_reverse(struct vnode *n)
{
	struct socket *s = (struct socket *) n->_vd;
	union sockaddr_union tmp;

	tmp = s->in.saddr;
	s->in.saddr = s->out.saddr;
	s->out.saddr = tmp;

	return 0;
}

int socket_stop(struct vnode *n)
{
	int ret;
	struct socket *s = (struct socket *) n->_vd;

	if (s->multicast.enabled) {
		ret = setsockopt(s->sd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &s->multicast.mreq, sizeof(s->multicast.mreq));
		if (ret)
			serror("Failed to leave multicast group");
	}

	if (s->sd >= 0) {
		ret = close(s->sd);
		if (ret)
			return ret;
	}

	ret = io_destroy(&s->io);
	if (ret)
		return ret;

	delete[] s->in.buf;
	delete[] s->out.buf;

	return 0;
}

int socket_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct socket *s = (struct socket *) n->_vd;

	char *ptr;
	ssize_t bytes;
	size_t rbytes;

	union sockaddr_union src;
	socklen_t srclen = sizeof(src);

	/* Receive next sample */
	bytes = recvfrom(s->sd, s->in.buf, s->in.buflen, 0, &src.sa, &srclen);
	if (bytes < 0)
		serror("Failed recv from node %s", node_name(n));
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
		warning("Received packet from unauthorized source: %s", buf);
		free(buf);

		return 0;
	}

	ret = io_sscan(&s->io, ptr, bytes, &rbytes, smps, cnt);
	if (ret < 0 || (size_t) bytes != rbytes)
		warning("Received invalid packet from node: %s ret=%d, bytes=%zu, rbytes=%zu", node_name(n), ret, bytes, rbytes);

	return ret;
}

int socket_write(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct socket *s = (struct socket *) n->_vd;

	int ret;
	ssize_t bytes;
	size_t wbytes;

retry:	ret = io_sprint(&s->io, s->out.buf, s->out.buflen, &wbytes, smps, cnt);
	if (ret < 0) {
		warning("Failed to format payload: reason=%d", ret);
		return ret;
	}

	if (wbytes == 0) {
		warning("Failed to format payload: wbytes=%zu", wbytes);
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
			warning("Failed send to node %s: %s", node_name(n), strerror(errno));
		else if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			warning("socket: send would block");
			goto retry2;
		}
		else
			warning("Failed sendto() to node %s", node_name(n));
	}
	else if ((size_t) bytes < wbytes)
		warning("Partial sendto() to node %s", node_name(n));

	return cnt;
}

int socket_parse(struct vnode *n, json_t *cfg)
{
	struct socket *s = (struct socket *) n->_vd;

	const char *local, *remote;
	const char *layer = nullptr;
	const char *format = "villas.binary";

	int ret;

	json_t *json_multicast = nullptr;
	json_error_t err;

	/* Default values */
	s->layer = SocketLayer::UDP;
	s->verify_source = 0;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s, s: { s: s }, s: { s: s, s?: b, s?: o } }",
		"layer", &layer,
		"format", &format,
		"out",
			"address", &remote,
		"in",
			"address", &local,
			"verify_source", &s->verify_source,
			"multicast", &json_multicast
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	/* Format */
	s->format = format_type_lookup(format);
	if (!s->format)
		error("Invalid format '%s' for node %s", format, node_name(n));

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
			error("Invalid layer '%s' for node %s", layer, node_name(n));
	}

	ret = socket_parse_address(remote, (struct sockaddr *) &s->out.saddr, s->layer, 0);
	if (ret) {
		error("Failed to resolve remote address '%s' of node %s: %s",
			remote, node_name(n), gai_strerror(ret));
	}

	ret = socket_parse_address(local, (struct sockaddr *) &s->in.saddr, s->layer, AI_PASSIVE);
	if (ret) {
		error("Failed to resolve local address '%s' of node %s: %s",
			local, node_name(n), gai_strerror(ret));
	}

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
			jerror(&err, "Failed to parse setting 'multicast' of node %s", node_name(n));

		ret = inet_aton(group, &s->multicast.mreq.imr_multiaddr);
		if (!ret) {
			error("Failed to resolve multicast group address '%s' of node %s",
				group, node_name(n));
		}

		if (interface) {
			ret = inet_aton(group, &s->multicast.mreq.imr_interface);
			if (!ret) {
				error("Failed to resolve multicast interface address '%s' of node %s",
					interface, node_name(n));
			}
		}
	}

	return 0;
}

int socket_fds(struct vnode *n, int fds[])
{
	struct socket *s = (struct socket *) n->_vd;

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
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.size		= sizeof(struct socket);
	p.node.type.start	= socket_type_start;
	p.node.reverse		= socket_reverse;
	p.node.parse		= socket_parse;
	p.node.print		= socket_print;
	p.node.check		= socket_check;
	p.node.start		= socket_start;
	p.node.stop		= socket_stop;
	p.node.read		= socket_read;
	p.node.write		= socket_write;
	p.node.poll_fds		= socket_fds;
	p.node.netem_fds	= socket_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
