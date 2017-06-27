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
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <endian.h>

#include "nodes/socket.h"
#include "config.h"
#include "utils.h"

#include "kernel/if.h"
#include "kernel/nl.h"
#include "kernel/tc.h"
#include "msg.h"
#include "msg_format.h"
#include "sample.h"
#include "queue.h"
#include "plugin.h"

/* Forward declartions */
static struct plugin p;

/* Private static storage */
struct list interfaces = { .state = STATE_DESTROYED };

int socket_init(struct super_node *sn)
{
	int ret;

	nl_init(); /* Fill link cache */
	list_init(&interfaces);

	/* Gather list of used network interfaces */
	for (size_t i = 0; i < list_length(&p.node.instances); i++) {
		struct node *n = list_at(&p.node.instances, i);
		struct socket *s = n->_vd;
		struct rtnl_link *link;

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
			i = list_at(&interfaces, k);

			if (rtnl_link_get_ifindex(i->nl_link) == rtnl_link_get_ifindex(link))
				goto found;
		}

		/* If not found, create a new interface */
		struct interface j = { .sockets.state = STATE_DESTROYED };

		ret = if_init(&j, link);
		if (ret)
			continue;

		i = memdup(&j, sizeof(j));
		list_push(&interfaces, i);

found:		list_push(&i->sockets, s);
	}

	for (size_t j = 0; j < list_length(&interfaces); j++) {
		struct interface *i = list_at(&interfaces, j);

		if_start(i);
	}

	return 0;
}

int socket_deinit()
{
	for (size_t j = 0; j < list_length(&interfaces); j++) {
		struct interface *i = list_at(&interfaces, j);

		if_stop(i);
	}

	list_destroy(&interfaces, (dtor_cb_t) if_destroy, false);

	return 0;
}

char * socket_print(struct node *n)
{
	struct socket *s = n->_vd;
	char *layer = NULL, *header = NULL, *endian = NULL, *buf;

	switch (s->layer) {
		case SOCKET_LAYER_UDP:	layer = "udp";	break;
		case SOCKET_LAYER_IP:	layer = "ip";	break;
		case SOCKET_LAYER_ETH:	layer = "eth";	break;
	}

	switch (s->header) {
		case SOCKET_HEADER_NONE:	header = "none";	break;
		case SOCKET_HEADER_FAKE:	header = "fake";	break;
		case SOCKET_HEADER_DEFAULT:	header = "default";	break;
	}

	if (s->header == SOCKET_HEADER_DEFAULT)
		endian = "auto";
	else {
		switch (s->endian) {
			case SOCKET_ENDIAN_LITTLE: endian = "little";	break;
			case SOCKET_ENDIAN_BIG:	   endian = "big";	break;
		}
	}

	char *local = socket_print_addr((struct sockaddr *) &s->local);
	char *remote = socket_print_addr((struct sockaddr *) &s->remote);

	buf = strf("layer=%s, header=%s, endian=%s, local=%s, remote=%s", layer, header, endian, local, remote);

	free(local);
	free(remote);

	return buf;
}

int socket_start(struct node *n)
{
	struct socket *s = n->_vd;
	int ret;
	
	/* Some checks on the addresses */
	if (s->local.sa.sa_family != s->remote.sa.sa_family)
		error("Address families of local and remote must match!");
	
	if (s->layer == SOCKET_LAYER_IP) {
		if (ntohs(s->local.sin.sin_port) != ntohs(s->remote.sin.sin_port))
			error("IP protocol numbers of local and remote must match!");
	}
	else if(s->layer == SOCKET_LAYER_ETH) {
		if (ntohs(s->local.sll.sll_protocol) != ntohs(s->remote.sll.sll_protocol))
			error("Ethertypes of local and remote must match!");
		
		if (ntohs(s->local.sll.sll_protocol) <= 0x5DC)
			error("Ethertype must be large than %d or it is interpreted as an IEEE802.3 length field!", 0x5DC);
	}

	/* Create socket */
	switch (s->layer) {
		case SOCKET_LAYER_UDP:	s->sd = socket(s->local.sa.sa_family, SOCK_DGRAM,	IPPROTO_UDP); break;
		case SOCKET_LAYER_IP:	s->sd = socket(s->local.sa.sa_family, SOCK_RAW,		ntohs(s->local.sin.sin_port)); break;
		case SOCKET_LAYER_ETH:	s->sd = socket(s->local.sa.sa_family, SOCK_DGRAM,	s->local.sll.sll_protocol); break;
		default:
			error("Invalid socket type!");
	}

	if (s->sd < 0)
		serror("Failed to create socket");

	/* Bind socket for receiving */
	ret = bind(s->sd, (struct sockaddr *) &s->local, sizeof(s->local));
	if (ret < 0)
		serror("Failed to bind socket");

	/* Set fwmark for outgoing packets if netem is enabled for this node */
	if (s->mark) {
		ret = setsockopt(s->sd, SOL_SOCKET, SO_MARK, &s->mark, sizeof(s->mark));
		if (ret)
			serror("Failed to set FW mark for outgoing packets");
		else
			debug(LOG_SOCKET | 4, "Set FW mark for socket (sd=%u) to %u", s->sd, s->mark);
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
			prio = SOCKET_PRIO;
			if (setsockopt(s->sd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
				serror("Failed to set socket priority");
			else
				debug(LOG_SOCKET | 4, "Set socket priority for node %s to %d", node_name(n), prio);
			break;
	}

	return 0;
}

int socket_reverse(struct node *n)
{
	struct socket *s = n->_vd;
	union sockaddr_union tmp;

	tmp = s->local;
	s->local = s->remote;
	s->remote = tmp;

	return 0;
}

int socket_stop(struct node *n)
{
	struct socket *s = n->_vd;

	if (s->sd >= 0)
		close(s->sd);

	return 0;
}

int socket_destroy(struct node *n)
{
	struct socket *s = n->_vd;

	rtnl_qdisc_put(s->tc_qdisc);
	rtnl_cls_put(s->tc_classifier);

	return 0;
}

static int socket_read_none(struct node *n, struct sample *smps[], unsigned cnt)
{
	int length;
	struct socket *s = n->_vd;
	
	char buf[MSG_MAX_PACKET_LEN];
	uint32_t *values = (uint32_t *) buf;
	ssize_t bytes;

	struct sample *smp = smps[0];

	if (cnt < 1)
		return 0;

	union sockaddr_union src;
	socklen_t srclen = sizeof(src);
	
	/* Receive next sample */
	bytes = recvfrom(s->sd, buf, sizeof(buf), 0, &src.sa, &srclen);
	if (bytes == 0)
		error("Remote node %s closed the connection", node_name(n)); /** @todo Should we really hard fail here? */
	else if (bytes < 0)
		serror("Failed recv from node %s", node_name(n));
	else if (bytes % 4 != 0) {
		warn("Packet size is invalid: %zd Must be multiple of 4 bytes.", bytes);
		recv(s->sd, NULL, 0, 0); /* empty receive buffer */
		return -1;
	}
	
	length = bytes / 4;
	
	/* Strip IP header from packet */
	if (s->layer == SOCKET_LAYER_IP) {
		struct ip *iphdr = (struct ip *) buf;
		
		length -= iphdr->ip_hl;
		values += iphdr->ip_hl;
	}
	
	/* SOCK_RAW IP sockets to not provide the IP protocol number via recvmsg()
	 * So we simply set it ourself. */
	if (s->layer == SOCKET_LAYER_IP) {
		switch (src.sa.sa_family) {
			case AF_INET: src.sin.sin_port = s->remote.sin.sin_port; break;
			case AF_INET6: src.sin6.sin6_port = s->remote.sin6.sin6_port; break;
		}
	}

	if (s->verify_source && socket_compare_addr(&src.sa, &s->remote.sa) != 0) {
		char *buf = socket_print_addr((struct sockaddr *) &src);
		warn("Received packet from unauthorized source: %s", buf);
		free(buf);

		return 0;
	}

	/* Convert packet contents to host endianess */
	for (int i = 0; i < length; i++)
		values[i] = s->endian == SOCKET_ENDIAN_BIG
				? be32toh(values[i])
				: le32toh(values[i]);

	if (s->header == SOCKET_HEADER_FAKE) {
		if (length < 3) {
			warn("Node %s received a packet with no fake header. Skipping...", node_name(n));
			return 0;
		}

		smp->sequence          = values[0];
		smp->ts.origin.tv_sec  = values[1];
		smp->ts.origin.tv_nsec = values[2];

		values += 3;
		length -= 3;
	}
	else {
		smp->sequence = n->sequence++; /* Fake sequence no generated by VILLASnode */
		smp->ts.origin.tv_sec  = 0;
		smp->ts.origin.tv_nsec = 0;
	}

	if (length > smp->capacity) {
		warn("Node %s received more values than supported. Dropping %u values", node_name(n), length - smp->capacity);
		length = smp->capacity;
	}

	memcpy(smp->data, values, SAMPLE_DATA_LEN(length));

	smp->ts.received.tv_sec  = 0;
	smp->ts.received.tv_nsec = 0;

	smp->length = length;

	return 1; /* GTNET-SKT sends every sample in a single packet */
}

static int socket_read_villas(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct socket *s = n->_vd;

	char buf[MSG_MAX_PACKET_LEN];
	char *bufptr = buf;
	ssize_t bytes;

	union sockaddr_union src;
	socklen_t srclen = sizeof(src);

	/* Receive next sample */
	bytes = recvfrom(s->sd, bufptr, sizeof(buf), 0, &src.sa, &srclen);
	if (bytes == 0)
		error("Remote node %s closed the connection", node_name(n)); /** @todo Should we really hard fail here? */
	else if (bytes < 0)
		serror("Failed recv from node %s", node_name(n));
	else if (bytes % 4 != 0) {
		warn("Packet size is invalid: %zd Must be multiple of 4 bytes.", bytes);
		recv(s->sd, NULL, 0, 0); /* empty receive buffer */
		return -1;
	}
	
	/* Strip IP header from packet */
	if (s->layer == SOCKET_LAYER_IP) {
		struct ip *iphdr = (struct ip *) bufptr;
		
		bytes  -= iphdr->ip_hl * 4;
		bufptr += iphdr->ip_hl * 4;
	}
	
	/* SOCK_RAW IP sockets to not provide the IP protocol number via recvmsg()
	 * So we simply set it ourself. */
	if (s->layer == SOCKET_LAYER_IP) {
		switch (src.sa.sa_family) {
			case AF_INET: src.sin.sin_port = s->remote.sin.sin_port; break;
			case AF_INET6: src.sin6.sin6_port = s->remote.sin6.sin6_port; break;
		}
	}

	if (s->verify_source && socket_compare_addr(&src.sa, &s->remote.sa) != 0) {
		char *buf = socket_print_addr((struct sockaddr *) &src);
		warn("Received packet from unauthorized source: %s", buf);
		free(buf);

		return 0;
	}

	ret = msg_buffer_to_samples(smps, cnt, bufptr, bytes);
	if (ret < 0)
		warn("Received invalid packet from node: %s reason=%d", node_name(n), ret);

	return ret;
}

static int socket_write_none(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct socket *s = n->_vd;

	int sent = 0;
	ssize_t bytes;

	if (cnt < 1)
		return 0;

	for (int i = 0; i < cnt; i++) {
		int off = s->header == SOCKET_HEADER_FAKE ? 3 : 0;
		int len = smps[i]->length + off;
		uint32_t data[len];

		/* First three values are sequence, seconds and nano-seconds timestamps */
		if (s->header == SOCKET_HEADER_FAKE) {
			data[0] = smps[i]->sequence;
			data[1] = smps[i]->ts.origin.tv_sec;
			data[2] = smps[i]->ts.origin.tv_nsec;
		}

		for (int j = 0; j < smps[i]->length; j++)
			data[off + j] = s->endian == SOCKET_ENDIAN_BIG
					? htobe32(smps[i]->data[j].i)
					: htole32(smps[i]->data[j].i);

		bytes = sendto(s->sd, data, len * sizeof(data[0]), 0,
			(struct sockaddr *) &s->remote, sizeof(s->remote));
		if (bytes < 0)
			serror("Failed send to node %s", node_name(n));

		sent++;
	}

	return sent;
}

static int socket_write_villas(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct socket *s = n->_vd;

	char data[MSG_MAX_PACKET_LEN];
	ssize_t bytes = 0, sent;

	sent = msg_buffer_from_samples(smps, cnt, data, sizeof(data));
	if (sent < 0)
		return -1;

	/* Send message */
	bytes = sendto(s->sd, data, sent, 0, (struct sockaddr *) &s->remote, sizeof(s->remote));
	if (bytes < 0)
		serror("Failed send to node %s", node_name(n));

	return cnt;
}

int socket_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct socket *s = n->_vd;

	switch (s->header) {
		case SOCKET_HEADER_NONE:
		case SOCKET_HEADER_FAKE:
			return socket_read_none(n, smps, cnt);

		case SOCKET_HEADER_DEFAULT:
			return socket_read_villas(n, smps, cnt);
	}

	return -1;
}

int socket_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct socket *s = n->_vd;

	switch (s->header) {
		case SOCKET_HEADER_NONE:
		case SOCKET_HEADER_FAKE:
			return socket_write_none(n, smps, cnt);

		case SOCKET_HEADER_DEFAULT:
			return socket_write_villas(n, smps, cnt);
	}

	return -1;
}

int socket_parse(struct node *n, config_setting_t *cfg)
{
	config_setting_t *cfg_netem;
	const char *local, *remote, *layer, *hdr, *endian;
	int ret;

	struct socket *s = n->_vd;

	/* IP layer */
	if (!config_setting_lookup_string(cfg, "layer", &layer))
		s->layer = SOCKET_LAYER_UDP;
	else {
		if (!strcmp(layer, "eth"))
			s->layer = SOCKET_LAYER_ETH;
		else if (!strcmp(layer, "ip"))
			s->layer = SOCKET_LAYER_IP;
		else if (!strcmp(layer, "udp"))
			s->layer = SOCKET_LAYER_UDP;
		else
			cerror(cfg, "Invalid layer '%s' for node %s", layer, node_name(n));
	}

	/* Application header */
	if (!config_setting_lookup_string(cfg, "header", &hdr))
		s->header = SOCKET_HEADER_DEFAULT;
	else {
		if      (!strcmp(hdr, "gtnet-skt") || (!strcmp(hdr, "none")))
			s->header = SOCKET_HEADER_NONE;
		else if (!strcmp(hdr, "gtnet-skt:fake") || (!strcmp(hdr, "fake")))
			s->header = SOCKET_HEADER_FAKE;
		else if (!strcmp(hdr, "villas") || !strcmp(hdr, "default"))
			s->header = SOCKET_HEADER_DEFAULT;
		else
			cerror(cfg, "Invalid application header type '%s' for node %s", hdr, node_name(n));
	}

	if (!config_setting_lookup_string(cfg, "endian", &endian))
		s->endian = SOCKET_ENDIAN_BIG;
	else {
		if      (!strcmp(endian, "big") || !strcmp(endian, "network"))
			s->endian = SOCKET_ENDIAN_BIG;
		else if (!strcmp(endian, "little"))
			s->endian = SOCKET_ENDIAN_LITTLE;
		else
			cerror(cfg, "Invalid endianness type '%s' for node %s", endian, node_name(n));
	}

	if (!config_setting_lookup_string(cfg, "remote", &remote))
		cerror(cfg, "Missing remote address for node %s", node_name(n));

	if (!config_setting_lookup_string(cfg, "local", &local))
		cerror(cfg, "Missing local address for node %s", node_name(n));

	if (!config_setting_lookup_bool(cfg, "verify_source", &s->verify_source))
		s->verify_source = 0;

	ret = socket_parse_addr(local, (struct sockaddr *) &s->local, s->layer, AI_PASSIVE);
	if (ret) {
		cerror(cfg, "Failed to resolve local address '%s' of node %s: %s",
			local, node_name(n), gai_strerror(ret));
	}

	ret = socket_parse_addr(remote, (struct sockaddr *) &s->remote, s->layer, 0);
	if (ret) {
		cerror(cfg, "Failed to resolve remote address '%s' of node %s: %s",
			remote, node_name(n), gai_strerror(ret));
	}

	cfg_netem = config_setting_get_member(cfg, "netem");
	if (cfg_netem) {
		int enabled = 1;
		if (!config_setting_lookup_bool(cfg_netem, "enabled", &enabled) || enabled)
			tc_parse(cfg_netem, &s->tc_qdisc);
		else
			s->tc_qdisc = NULL;
	}
	else
		s->tc_qdisc = NULL;

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

		case AF_PACKET:
			strcatf(&buf, "%02x", sa->sll.sll_addr[0]);
			for (int i = 1; i < sa->sll.sll_halen; i++)
				strcatf(&buf, ":%02x", sa->sll.sll_addr[i]);
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

		case AF_PACKET: {
			struct nl_cache *cache = nl_cache_mngt_require("route/link");
			struct rtnl_link *link = rtnl_link_get(cache, sa->sll.sll_ifindex);
			if (!link)
				error("Failed to get interface for index: %u", sa->sll.sll_ifindex);

			strcatf(&buf, "%%%s", rtnl_link_get_name(link));
			strcatf(&buf, ":%hu", ntohs(sa->sll.sll_protocol));
			break;
		}
	}

	return buf;
}

int socket_parse_addr(const char *addr, struct sockaddr *saddr, enum socket_layer layer, int flags)
{
	/** @todo: Add support for IPv6 */
	union sockaddr_union *sa = (union sockaddr_union *) saddr;

	char *copy = strdup(addr);
	int ret;

	if (layer == SOCKET_LAYER_ETH) { /* Format: "ab:cd:ef:12:34:56%ifname:protocol" */
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

		case AF_PACKET:
			CMP(ntohs(xu->sll.sll_protocol), ntohs(yu->sll.sll_protocol));
			CMP(xu->sll.sll_ifindex, yu->sll.sll_ifindex);
//			CMP(xu->sll.sll_pkttype, yu->sll.sll_pkttype);
//			CMP(xu->sll.sll_hatype, yu->sll.sll_hatype);

			CMP(xu->sll.sll_halen, yu->sll.sll_halen);
			return memcmp(xu->sll.sll_addr, yu->sll.sll_addr, xu->sll.sll_halen);

		default:
			return -1;
	}

#undef CMP
}

static struct plugin p = {
	.name		= "socket",
	.description	= "BSD network sockets",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct socket),
		.destroy	= socket_destroy,
		.reverse	= socket_reverse,
		.parse		= socket_parse,
		.print		= socket_print,
		.start		= socket_start,
		.stop		= socket_stop,
		.read		= socket_read,
		.write		= socket_write,
		.init		= socket_init,
		.deinit		= socket_deinit,
		.instances	= LIST_INIT()
	}
};

REGISTER_PLUGIN(&p)
