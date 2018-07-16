/** Various socket related functions
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

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <errno.h>

#if defined(__linux__)
  #include <netinet/ether.h>
#endif

#include <villas/nodes/socket.h>
#include <villas/config.h>
#include <villas/utils.h>
#include <villas/format_type.h>
#include <villas/sample.h>
#include <villas/queue.h>
#include <villas/plugin.h>
#include <villas/compat.h>

#ifdef WITH_NETEM
  #include <villas/kernel/if.h>
  #include <villas/kernel/nl.h>
  #include <villas/kernel/tc_netem.h>
#endif /* WITH_NETEM */

/* Forward declartions */
static struct plugin p;

/* Private static storage */
struct list interfaces = { .state = STATE_DESTROYED };

int socket_type_start(struct super_node *sn)
{
#ifdef WITH_NETEM
	int ret;

	nl_init(); /* Fill link cache */
	list_init(&interfaces);

	/* Gather list of used network interfaces */
	for (size_t i = 0; i < list_length(&p.node.instances); i++) {
		struct node *n = (struct node *) list_at(&p.node.instances, i);
		struct socket *s = (struct socket *) n->_vd;
		struct rtnl_link *link;

		if (s->layer != SOCKET_LAYER_ETH &&
		    s->layer != SOCKET_LAYER_IP &&
		    s->layer != SOCKET_LAYER_UDP)
			    continue;

		/* Determine outgoing interface */
		ret = if_get_egress((struct sockaddr *) &s->remote, &link);
		if (ret) {
			char *buf = socket_print_addr((struct sockaddr *) &s->remote);
			error("Failed to get interface for socket address '%s'", buf);
			free(buf);
		}

		/* Search of existing interface with correct ifindex */
		struct interface *i;

		for (size_t k = 0; k < list_length(&interfaces); k++) {
			i = (struct interface *) list_at(&interfaces, k);

			if (rtnl_link_get_ifindex(i->nl_link) == rtnl_link_get_ifindex(link))
				goto found;
		}

		/* If not found, create a new interface */
		i = alloc(sizeof(struct interface));

		ret = if_init(i, link);
		if (ret)
			continue;

		list_push(&interfaces, i);

found:		list_push(&i->sockets, s);
	}

	for (size_t j = 0; j < list_length(&interfaces); j++) {
		struct interface *i = (struct interface *) list_at(&interfaces, j);

		if_start(i);
	}
#endif /* WITH_NETEM */

	return 0;
}

int socket_type_stop()
{
#ifdef WITH_NETEM
	for (size_t j = 0; j < list_length(&interfaces); j++) {
		struct interface *i = (struct interface *) list_at(&interfaces, j);

		if_stop(i);
	}

	list_destroy(&interfaces, (dtor_cb_t) if_destroy, false);
#endif /* WITH_NETEM */

	return 0;
}

char * socket_print(struct node *n)
{
	struct socket *s = (struct socket *) n->_vd;
	char *layer = NULL, *buf;

	switch (s->layer) {
		case SOCKET_LAYER_UDP:	layer = "udp";	break;
		case SOCKET_LAYER_IP:	layer = "ip";	break;
		case SOCKET_LAYER_ETH:	layer = "eth";	break;
		case SOCKET_LAYER_UNIX:	layer = "unix";	break;
	}

	char *local = socket_print_addr((struct sockaddr *) &s->local);
	char *remote = socket_print_addr((struct sockaddr *) &s->remote);

	buf = strf("layer=%s, format=%s, local=%s, remote=%s", layer, plugin_name(s->format), local, remote);

	if (s->multicast.enabled) {
		char group[INET_ADDRSTRLEN];
		char interface[INET_ADDRSTRLEN];

		inet_ntop(AF_INET, &s->multicast.mreq.imr_multiaddr, group, sizeof(group));
		inet_ntop(AF_INET, &s->multicast.mreq.imr_interface, interface, sizeof(interface));

		strcatf(&buf, ", multicast.enabled=%s", s->multicast.enabled ? "yes" : "no");
		strcatf(&buf, ", multicast.loop=%s",    s->multicast.loop    ? "yes" : "no");
		strcatf(&buf, ", multicast.group=%s", group);
		strcatf(&buf, ", multicast.interface=%s", s->multicast.mreq.imr_interface.s_addr == INADDR_ANY ? "any" : interface);
		strcatf(&buf, ", multicast.ttl=%u", s->multicast.ttl);
	}

	free(local);
	free(remote);

	return buf;
}

int socket_start(struct node *n)
{
	struct socket *s = (struct socket *) n->_vd;
	int ret;

	// TODO: Move to socket_check() ?
	/* Some checks on the addresses */
	if (s->layer != SOCKET_LAYER_UNIX) {
		if (s->local.sa.sa_family != s->remote.sa.sa_family)
			error("Address families of local and remote must match!");
	}

	if (s->layer == SOCKET_LAYER_IP) {
		if (ntohs(s->local.sin.sin_port) != ntohs(s->remote.sin.sin_port))
			error("IP protocol numbers of local and remote must match!");
	}
#ifdef __linux__
	else if (s->layer == SOCKET_LAYER_ETH) {
		if (ntohs(s->local.sll.sll_protocol) != ntohs(s->remote.sll.sll_protocol))
			error("Ethertypes of local and remote must match!");

		if (ntohs(s->local.sll.sll_protocol) <= 0x5DC)
			error("Ethertype must be large than %d or it is interpreted as an IEEE802.3 length field!", 0x5DC);
	}
#endif /* __linux__ */

	if (s->multicast.enabled) {
		if (s->local.sa.sa_family != AF_INET)
			error("Multicast is only supported by IPv4 for node %s", node_name(n));

		uint32_t addr = ntohl(s->multicast.mreq.imr_multiaddr.s_addr);
		if ((addr >> 28) != 14)
			error("Multicast group address of node %s must be within 224.0.0.0/4", node_name(n));
	}

	/* Initialize IO */
	ret = io_init(&s->io, s->format, n, SAMPLE_HAS_ALL);
	if (ret)
		return ret;

	/* Create socket */
	switch (s->layer) {
		case SOCKET_LAYER_UDP:	s->sd = socket(s->local.sa.sa_family, SOCK_DGRAM,	IPPROTO_UDP); break;
		case SOCKET_LAYER_IP:	s->sd = socket(s->local.sa.sa_family, SOCK_RAW,		ntohs(s->local.sin.sin_port)); break;
#ifdef __linux__
		case SOCKET_LAYER_ETH:	s->sd = socket(s->local.sa.sa_family, SOCK_DGRAM,	s->local.sll.sll_protocol); break;
#endif /* __linux__ */
		case SOCKET_LAYER_UNIX:	s->sd = socket(s->local.sa.sa_family, SOCK_DGRAM,	0); break;
		default:
			error("Invalid socket type!");
	}

	if (s->sd < 0)
		serror("Failed to create socket");

	/* Bind socket for receiving */
	socklen_t addrlen = 0;
	if (s->layer == SOCKET_LAYER_UNIX) {
		ret = unlink(s->local.sun.sun_path);
		if (ret && errno != ENOENT)
			return ret;

		addrlen = SUN_LEN(&s->local.sun);
	}
	else
		addrlen = sizeof(s->local);

	ret = bind(s->sd, (struct sockaddr *) &s->local, addrlen);
	if (ret < 0)
		serror("Failed to bind socket");

#ifdef __linux__
	/* Set fwmark for outgoing packets if netem is enabled for this node */
	if (s->mark) {
		ret = setsockopt(s->sd, SOL_SOCKET, SO_MARK, &s->mark, sizeof(s->mark));
		if (ret)
			serror("Failed to set FW mark for outgoing packets");
		else
			debug(LOG_SOCKET | 4, "Set FW mark for socket (sd=%u) to %u", s->sd, s->mark);
	}
#endif /* __linux__ */

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
		case SOCKET_LAYER_UDP:
		case SOCKET_LAYER_IP:
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

	return 0;
}

int socket_reverse(struct node *n)
{
	struct socket *s = (struct socket *) n->_vd;
	union sockaddr_union tmp;

	tmp = s->local;
	s->local = s->remote;
	s->remote = tmp;

	return 0;
}

int socket_stop(struct node *n)
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

	return 0;
}

int socket_destroy(struct node *n)
{
	int ret;
	struct socket *s = (struct socket *) n->_vd;

	ret = io_destroy(&s->io);
	if (ret)
		return ret;

#ifdef WITH_NETEM
	rtnl_qdisc_put(s->tc_qdisc);
	rtnl_cls_put(s->tc_classifier);
#endif /* WITH_NETEM */

	return 0;
}

int socket_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	struct socket *s = (struct socket *) n->_vd;

	char *buf, *ptr;
	ssize_t buflen;
	ssize_t bytes;
	size_t rbytes;

	union sockaddr_union src;
	socklen_t srclen = sizeof(src);

	/* Get size of next packet */
	buflen = recvfrom(s->sd, NULL, 0, MSG_TRUNC | MSG_PEEK, &src.sa, &srclen);
	if (buflen < 0)
		return -1;

	buf = alloc(buflen);
	if (!buf)
		return -1;

	/* Receive next sample */
	bytes = recvfrom(s->sd, buf, buflen, 0, &src.sa, &srclen);
	if (bytes < 0)
		serror("Failed recv from node %s", node_name(n));

	ptr = buf;

	/* Strip IP header from packet */
	if (s->layer == SOCKET_LAYER_IP) {
		struct ip *iphdr = (struct ip *) ptr;

		bytes -= iphdr->ip_hl * 4;
		ptr += iphdr->ip_hl * 4;
	}

	/* SOCK_RAW IP sockets to not provide the IP protocol number via recvmsg()
	 * So we simply set it ourself. */
	if (s->layer == SOCKET_LAYER_IP) {
		switch (src.sa.sa_family) {
			case AF_INET:  src.sin.sin_port   = s->remote.sin.sin_port;   break;
			case AF_INET6: src.sin6.sin6_port = s->remote.sin6.sin6_port; break;
		}
	}

	if (s->verify_source && socket_compare_addr(&src.sa, &s->remote.sa) != 0) {
		char *buf = socket_print_addr((struct sockaddr *) &src);
		warn("Received packet from unauthorized source: %s", buf);
		free(buf);

		ret = 0;
		goto out;
	}

	ret = io_sscan(&s->io, ptr, bytes, &rbytes, smps, cnt);

	if (ret < 0 || bytes != rbytes)
		warn("Received invalid packet from node: %s ret=%d, bytes=%zu, rbytes=%zu", node_name(n), ret, bytes, rbytes);

out:	free(buf);

	return ret;
}

int socket_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct socket *s = (struct socket *) n->_vd;

	int ret;
	char *buf;
	size_t buflen;
	ssize_t bytes;
	size_t wbytes;

	buflen = SOCKET_INITIAL_BUFFER_LEN;
	buf = alloc(buflen);
	if (!buf)
		return -1;

retry:	ret = io_sprint(&s->io, buf, buflen, &wbytes, smps, cnt);
	if (ret < 0)
		goto out;

	if (wbytes <= 0)
		goto out;

	if (wbytes > buflen) {
		buflen = wbytes;
		buf = realloc(buf, buflen);
		goto retry;
	}

	/* Send message */
	socklen_t addrlen = 0;
	switch (s->layer) {
		case SOCKET_LAYER_UDP:
		case SOCKET_LAYER_IP:
		case SOCKET_LAYER_ETH:	addrlen = sizeof(s->remote); break;
		case SOCKET_LAYER_UNIX:	addrlen = SUN_LEN(&s->remote.sun); break;
	}

	bytes = sendto(s->sd, buf, wbytes, MSG_DONTWAIT, (struct sockaddr *) &s->remote, addrlen);
	if (bytes < 0) {
		if ((errno == EPERM) ||
		    (errno == ENOENT && s->layer == SOCKET_LAYER_UNIX))
			warn("Failed send to node %s: %s", node_name(n), strerror(errno));
		else if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			warn("socket: send would block");
		else
			serror("Failed send to node %s", node_name(n));
	}

	if (bytes != wbytes)
		warn("Partial send to node %s", node_name(n));

out:	free(buf);

	return ret;
}

int socket_parse(struct node *n, json_t *cfg)
{
	struct socket *s = (struct socket *) n->_vd;

	const char *local, *remote;
	const char *layer = NULL;
	const char *format = "villas.binary";

	int ret;

	json_t *json_multicast = NULL;
	json_t *json_netem = NULL;
	json_error_t err;

	/* Default values */
	s->layer = SOCKET_LAYER_UDP;
	s->verify_source = 0;
#ifdef WITH_NETEM
	s->tc_qdisc = NULL;
#endif /* WITH_NETEM */

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s: s, s: s, s?: b, s?: o, s?: o, s?: s }",
		"layer", &layer,
		"remote", &remote,
		"local", &local,
		"verify_source", &s->verify_source,
		"multicast", &json_multicast,
		"netem", &json_netem,
		"format", &format
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
			s->layer = SOCKET_LAYER_IP;
#ifdef __linux__
		else if (!strcmp(layer, "eth"))
			s->layer = SOCKET_LAYER_ETH;
#endif /*__linux__ */
		else if (!strcmp(layer, "udp"))
			s->layer = SOCKET_LAYER_UDP;
		else if (!strcmp(layer, "unix") || !strcmp(layer, "local"))
			s->layer = SOCKET_LAYER_UNIX;
		else
			error("Invalid layer '%s' for node %s", layer, node_name(n));
	}

	ret = socket_parse_addr(remote, (struct sockaddr *) &s->remote, s->layer, 0);
	if (ret) {
		error("Failed to resolve remote address '%s' of node %s: %s",
			remote, node_name(n), gai_strerror(ret));
	}

	ret = socket_parse_addr(local, (struct sockaddr *) &s->local, s->layer, AI_PASSIVE);
	if (ret) {
		error("Failed to resolve local address '%s' of node %s: %s",
			local, node_name(n), gai_strerror(ret));
	}

	if (json_multicast) {
		const char *group, *interface = NULL;

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

	if (json_netem) {
#ifdef WITH_NETEM
		int enabled = 1;

		ret = json_unpack_ex(json_netem, &err, 0, "{ s?: b }",  "enabled", &enabled);
		if (ret)
			jerror(&err, "Failed to parse setting 'netem' of node %s", node_name(n));

		if (enabled)
			tc_netem_parse(&s->tc_qdisc, json_netem);
		else
			s->tc_qdisc = NULL;
#endif /* WITH_NETEM */
	}

	return 0;
}

char * socket_print_addr(struct sockaddr *saddr)
{
	union sockaddr_union *sa = (union sockaddr_union *) saddr;
	char *buf = alloc(64);

	/* Address */
	switch (sa->sa.sa_family) {
		case AF_INET6:
			inet_ntop(AF_INET6, &sa->sin6.sin6_addr, buf, 64);
			break;

		case AF_INET:
			inet_ntop(AF_INET, &sa->sin.sin_addr, buf, 64);
			break;

#ifdef __linux__
		case AF_PACKET:
			strcatf(&buf, "%02x", sa->sll.sll_addr[0]);
			for (int i = 1; i < sa->sll.sll_halen; i++)
				strcatf(&buf, ":%02x", sa->sll.sll_addr[i]);
			break;
#endif /* __linux__ */
		case AF_UNIX:
			strcatf(&buf, "%s", sa->sun.sun_path);
			break;

		default:
			error("Unknown address family: '%u'", sa->sa.sa_family);
	}

	/* Port  / Interface */
	switch (sa->sa.sa_family) {
		case AF_INET6:
		case AF_INET:
			strcatf(&buf, ":%hu", ntohs(sa->sin.sin_port));
			break;

#ifdef __linux__
		case AF_PACKET: {
			struct nl_cache *cache = nl_cache_mngt_require("route/link");
			struct rtnl_link *link = rtnl_link_get(cache, sa->sll.sll_ifindex);
			if (!link)
				error("Failed to get interface for index: %u", sa->sll.sll_ifindex);

			strcatf(&buf, "%%%s", rtnl_link_get_name(link));
			strcatf(&buf, ":%hu", ntohs(sa->sll.sll_protocol));
			break;
		}
#endif /* __linux__ */
	}

	return buf;
}

int socket_parse_addr(const char *addr, struct sockaddr *saddr, enum socket_layer layer, int flags)
{
	/** @todo: Add support for IPv6 */
	union sockaddr_union *sa = (union sockaddr_union *) saddr;

	char *copy = strdup(addr);
	int ret;

	if (layer == SOCKET_LAYER_UNIX) { /* Format: "/path/to/socket" */
		sa->sun.sun_family = AF_UNIX;

		if (strlen(addr) > sizeof(sa->sun.sun_path)-1)
			error("Length of unix socket path is too long!");

		memcpy(sa->sun.sun_path, addr, strlen(sa->sun.sun_path)+1);

		ret = 0;
	}
#ifdef __linux__
	else if (layer == SOCKET_LAYER_ETH) { /* Format: "ab:cd:ef:12:34:56%ifname:protocol" */
		/* Split string */
		char *node = strtok(copy, "%");
		char *ifname = strtok(NULL, ":");
		char *proto = strtok(NULL, "\0");

		/* Parse link layer (MAC) address */
		struct ether_addr *mac = ether_aton(node);
		if (!mac)
			error("Failed to parse MAC address: %s", node);

		memcpy(&sa->sll.sll_addr, &mac->ether_addr_octet, ETHER_ADDR_LEN);

		/* Get interface index from name */
		nl_init();
		struct nl_cache *cache = nl_cache_mngt_require("route/link");
		struct rtnl_link *link = rtnl_link_get_by_name(cache, ifname);
		if (!link)
			error("Failed to get network interface: '%s'", ifname);

		sa->sll.sll_protocol = htons(proto ? strtol(proto, NULL, 0) : ETH_P_VILLAS);
		sa->sll.sll_halen = ETHER_ADDR_LEN;
		sa->sll.sll_family = AF_PACKET;
		sa->sll.sll_ifindex = rtnl_link_get_ifindex(link);

		ret = 0;
	}
#endif /* __linux__ */
	else {	/* Format: "192.168.0.10:12001" */
		struct addrinfo hint = {
			.ai_flags = flags,
			.ai_family = AF_UNSPEC
		};

		/* Split string */
		char *node = strtok(copy, ":");
		char *service = strtok(NULL, "\0");

		if (node && !strcmp(node, "*"))
			node = NULL;

		if (service && !strcmp(service, "*"))
			service = NULL;

		switch (layer) {
			case SOCKET_LAYER_IP:
				hint.ai_socktype = SOCK_RAW;
				hint.ai_protocol = (service) ? strtol(service, NULL, 0) : IPPROTO_VILLAS;
				hint.ai_flags |= AI_NUMERICSERV;
				break;

			case SOCKET_LAYER_UDP:
				hint.ai_socktype = SOCK_DGRAM;
				hint.ai_protocol = IPPROTO_UDP;
				break;

			default:
				error("Invalid address type");
		}

		/* Lookup address */
		struct addrinfo *result;
		ret = getaddrinfo(node, (layer == SOCKET_LAYER_IP) ? NULL : service, &hint, &result);
		if (!ret) {
			if (layer == SOCKET_LAYER_IP) {
				/* We mis-use the sin_port field to store the IP protocol number on RAW sockets */
				struct sockaddr_in *sin = (struct sockaddr_in *) result->ai_addr;
				sin->sin_port = htons(result->ai_protocol);
			}

			memcpy(sa, result->ai_addr, result->ai_addrlen);
			freeaddrinfo(result);
		}
	}

	free(copy);

	return ret;
}

int socket_compare_addr(struct sockaddr *x, struct sockaddr *y)
{
#define CMP(a, b) if (a != b) return a < b ? -1 : 1

	union sockaddr_union *xu = (void *) x, *yu = (void *) y;

	CMP(x->sa_family, y->sa_family);

	switch (x->sa_family) {
		case AF_UNIX:
			return strcmp(xu->sun.sun_path, yu->sun.sun_path);

		case AF_INET:
			CMP(ntohl(xu->sin.sin_addr.s_addr), ntohl(yu->sin.sin_addr.s_addr));
			CMP(ntohs(xu->sin.sin_port), ntohs(yu->sin.sin_port));

			return 0;

		case AF_INET6:
			CMP(ntohs(xu->sin6.sin6_port), ntohs(yu->sin6.sin6_port));
//			CMP(xu->sin6.sin6_flowinfo, yu->sin6.sin6_flowinfo);
//			CMP(xu->sin6.sin6_scope_id, yu->sin6.sin6_scope_id);

			return memcmp(xu->sin6.sin6_addr.s6_addr, yu->sin6.sin6_addr.s6_addr, sizeof(xu->sin6.sin6_addr.s6_addr));

#ifdef __linux__
		case AF_PACKET:
			CMP(ntohs(xu->sll.sll_protocol), ntohs(yu->sll.sll_protocol));
			CMP(xu->sll.sll_ifindex, yu->sll.sll_ifindex);
//			CMP(xu->sll.sll_pkttype, yu->sll.sll_pkttype);
//			CMP(xu->sll.sll_hatype, yu->sll.sll_hatype);

			CMP(xu->sll.sll_halen, yu->sll.sll_halen);
			return memcmp(xu->sll.sll_addr, yu->sll.sll_addr, xu->sll.sll_halen);
#endif /* __linux__ */

		default:
			return -1;
	}

#undef CMP
}

int socket_fd(struct node *n)
{
	struct socket *s = (struct socket *) n->_vd;

	return s->sd;
}

static struct plugin p = {
	.name		= "socket",
#ifdef WITH_NETEM
	.description	= "BSD network sockets for Ethernet / IP / UDP (libnl3, netem support)",
#else
	.description	= "BSD network sockets for Ethernet / IP / UDP",
#endif
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct socket),
		.type.start	= socket_type_start,
		.type.stop	= socket_type_stop,
		.destroy	= socket_destroy,
		.reverse	= socket_reverse,
		.parse		= socket_parse,
		.print		= socket_print,
		.start		= socket_start,
		.stop		= socket_stop,
		.read		= socket_read,
		.write		= socket_write,
		.fd		= socket_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)

