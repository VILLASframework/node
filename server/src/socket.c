/** Various socket related functions
 *
 * Parse and print addresses, connect, close, etc...
 *
 * S2SS uses these functions to setup the network emulation feature.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>

#include <netinet/ether.h>
#include <netinet/ip.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>

#include "if.h"
#include "nl.h"
#include "tc.h"
#include "config.h"
#include "utils.h"
#include "socket.h"

/** Linked list of interfaces */
extern struct list interfaces;

/** Linked list of all sockets nodes */
static struct list sockets;

int socket_init(int argc, char * argv[], struct settings *set)
{ INDENT
	nl_init(); /* Fill link cache */
	list_init(&interfaces, (dtor_cb_t) if_destroy);

	/* Gather list of used network interfaces */
	FOREACH(&sockets, it) {
		struct socket *s = it->socket;
		struct rtnl_link *link;

		/* Determine outgoing interface */
		if (if_get_egress((struct sockaddr *) &s->remote, &link)) {
			char buf[128];
			socket_print_addr(buf, sizeof(buf), (struct sockaddr *) &s->remote);
			error("Failed to get interface for socket address '%s'", buf);
		}

		int ifindex = rtnl_link_get_ifindex(link);
		struct interface *i = if_lookup_index(ifindex);
		if (!i)
			i = if_create(link);

		list_push(&i->sockets, s);
	}

	/** @todo Improve mapping of NIC IRQs per path */
	FOREACH(&interfaces, it)
		if_start(it->interface, set->affinity);

	return 0;
}

int socket_deinit()
{ INDENT
	FOREACH(&interfaces, it)
		if_stop(it->interface);

	list_destroy(&interfaces);

	return 0;
}

int socket_print(struct node *n, char *buf, int len)
{
	struct socket *s = n->socket;

	char local[INET6_ADDRSTRLEN + 16];
	char remote[INET6_ADDRSTRLEN + 16];
	char *layer = NULL;
	
	switch (s->layer) {
		case LAYER_UDP: layer = "udp"; break;
		case LAYER_IP:	layer = "ip"; break;
		case LAYER_ETH:	layer = "eth"; break;
	}

	socket_print_addr(local, sizeof(local), (struct sockaddr *) &s->local);
	socket_print_addr(remote, sizeof(remote), (struct sockaddr *) &s->remote);

	return snprintf(buf, len, "layer=%s, local=%s, remote=%s", layer, local, remote);
}

int socket_open(struct node *n)
{
	struct socket *s = n->socket;
	struct sockaddr_in *sin = (struct sockaddr_in *) &s->local;
	struct sockaddr_ll *sll = (struct sockaddr_ll *) &s->local;
	int ret;

	/* Create socket */
	switch (s->layer) {
		case LAYER_UDP:	s->sd = socket(sin->sin_family, SOCK_DGRAM,	IPPROTO_UDP); break;
		case LAYER_IP:	s->sd = socket(sin->sin_family, SOCK_RAW,	ntohs(sin->sin_port)); break;
		case LAYER_ETH:	s->sd = socket(sll->sll_family, SOCK_DGRAM,	sll->sll_protocol); break;
		default:
			error("Invalid socket type!");
	}

	if (s->sd < 0)
		serror("Failed to create socket");

	/* Bind socket for receiving */
	ret = bind(s->sd, (struct sockaddr *) &s->local, sizeof(s->local));
	if (ret < 0)
		serror("Failed to bind socket");

	/* Set fwmark for outgoing packets */
	if (setsockopt(s->sd, SOL_SOCKET, SO_MARK, &s->mark, sizeof(s->mark)))
		serror("Failed to set FW mark for outgoing packets");
	else
		debug(4, "Set FW mark for socket (sd=%u) to %u", s->sd, s->mark);

	/* Set socket priority, QoS or TOS IP options */
	int prio;
	switch (s->layer) {
		case LAYER_UDP:
		case LAYER_IP:
			prio = IPTOS_LOWDELAY;
			if (setsockopt(s->sd, IPPROTO_IP, IP_TOS, &prio, sizeof(prio)))
				serror("Failed to set type of service (QoS)");
			else
				debug(4, "Set QoS/TOS IP option for node '%s' to %#x", n->name, prio);
			break;

		default:
			prio = SOCKET_PRIO;
			if (setsockopt(s->sd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
				serror("Failed to set socket priority");
			else
				debug(4, "Set socket priority for node '%s' to %u", n->name, prio);
			break;
	}

	return 0;
}

int socket_close(struct node *n)
{
	struct socket *s = n->socket;

	if (s->sd >= 0)
		close(s->sd);

	return 0;
}

int socket_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct socket *s = n->socket;

	int bytes;
	struct iovec iov[cnt];
	struct msghdr mhdr = {
		.msg_iov = iov,
		.msg_iovlen = ARRAY_LEN(iov)
	};

	/* Wait until next packet received */
	poll(&(struct pollfd) { .fd = s->sd, .events = POLLIN }, 1, -1);
	/* Get size of received packet in bytes */
	ioctl(s->sd, FIONREAD, &bytes);

	/* Check if packet length is correct */
	if (bytes % (cnt * 4) != 0)
		error("Packet length not dividable by 4: received=%u, cnt=%u", bytes, cnt);
	if (bytes / cnt > sizeof(struct msg))
		error("Packet length is too large: received=%u, cnt=%u, max=%zu", bytes, cnt, sizeof(struct msg));

	for (int i = 0; i < cnt; i++) {
		/* All messages of a packet must have equal length! */
		iov[i].iov_base = &pool[(first+poolsize+i) % poolsize];
		iov[i].iov_len  = bytes / cnt;
	}

	/* Receive message from socket */
	bytes = recvmsg(s->sd, &mhdr, 0);
	if (bytes == 0)
		error("Remote node '%s' closed the connection", n->name);
	else if (bytes < 0)
		serror("Failed recv");

	debug(10, "Received packet of %u bytes: %u samples a %u values per sample", bytes, cnt, (bytes / cnt) / 4 - 4);

	for (int i = 0; i < cnt; i++) {
		struct msg *n = &pool[(first+poolsize+i) % poolsize];

		/* Check integrity of packet */
		bytes -= MSG_LEN(n);

		/* Convert headers to host byte order */
		n->sequence = ntohs(n->sequence);

		/* Convert message to host endianess */
		if (n->endian != MSG_ENDIAN_HOST)
			msg_swap(n);
	}

	/* Check packet integrity */
	if (bytes != 0)
		error("Packet length does not match message header length! %u bytes left over.", bytes);

	return cnt;
}

int socket_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct socket *s = n->socket;
	int bytes, sent = 0;

	/** @todo we should check the MTU */

	struct iovec iov[cnt];
	for (int i = 0; i < cnt; i++) {
		struct msg *n = &pool[(first+i) % poolsize];

		if (n->type == MSG_TYPE_EMPTY)
			continue;

		/* Convert headers to network byte order */
		n->sequence = htons(n->sequence);

		iov[sent].iov_base = n;
		iov[sent].iov_len  = MSG_LEN(n);

		sent++;
	}

	struct msghdr mhdr = {
		.msg_iov = iov,
		.msg_iovlen = sent
	};

	/* Specify destination address for connection-less procotols */
	switch (s->layer) {
		case LAYER_UDP:
		case LAYER_IP:
		case LAYER_ETH:
			mhdr.msg_name = (struct sockaddr *) &s->remote;
			mhdr.msg_namelen = sizeof(s->remote);
			break;
	}

	bytes = sendmsg(s->sd, &mhdr, 0);
	if (bytes < 0)
		serror("Failed send");

	debug(10, "Sent packet of %u bytes: %u samples a %u values per sample", bytes, cnt, (bytes / cnt) / 4 - 4);

	return sent;
}

int socket_parse(config_setting_t *cfg, struct node *n)
{
	const char *local, *remote, *layer;
	int ret;

	struct socket *s = alloc(sizeof(struct socket));

	if (!config_setting_lookup_string(cfg, "layer", &layer))
		cerror(cfg, "Missing layer setting for node '%s'", n->name);

	if (!strcmp(layer, "eth"))
		s->layer = LAYER_ETH;
	else if (!strcmp(layer, "ip"))
		s->layer = LAYER_IP;
	else if (!strcmp(layer, "udp"))
		s->layer = LAYER_UDP;
	else
		cerror(cfg, "Invalid layer '%s' for node '%s'", layer, n->name);

	if (!config_setting_lookup_string(cfg, "remote", &remote))
		cerror(cfg, "Missing remote address for node '%s'", n->name);

	if (!config_setting_lookup_string(cfg, "local", &local))
		cerror(cfg, "Missing local address for node '%s'", n->name);
	
	ret = socket_parse_addr(local, (struct sockaddr *) &s->local, s->layer, AI_PASSIVE);
	if (ret) {
		cerror(cfg, "Failed to resolve local address '%s' of node '%s': %s",
			local, n->name, gai_strerror(ret));
	}

	ret = socket_parse_addr(remote, (struct sockaddr *) &s->remote, s->layer, 0);
	if (ret) {
		cerror(cfg, "Failed to resolve remote address '%s' of node '%s': %s",
			remote, n->name, gai_strerror(ret));
	}

	config_setting_t *cfg_netem = config_setting_get_member(cfg, "netem");
	if (cfg_netem) {
		int enabled = 1;
		if (!config_setting_lookup_bool(cfg_netem, "enabled", &enabled) || enabled)
			tc_parse(cfg_netem, &s->tc_qdisc);
	}

	n->socket = s;

	list_push(&sockets, s);

	return 0;
}

int socket_print_addr(char *buf, int len, struct sockaddr *saddr)
{
	union sockaddr_union *sa = (union sockaddr_union *) saddr;
	
	/* Address */
	switch (sa->sa.sa_family) {
		case AF_INET6:
			inet_ntop(AF_INET6, &sa->sin6.sin6_addr, buf, len);
			break;

		case AF_INET:
			inet_ntop(AF_INET, &sa->sin.sin_addr, buf, len);
			break;
			
		case AF_PACKET:
			snprintf(buf, len, "%02x", sa->sll.sll_addr[0]);
			for (int i = 1; i < sa->sll.sll_halen; i++)
				strap(buf, len, ":%02x", sa->sll.sll_addr[i]);
			break;

		default:
			error("Unknown address family: '%u'", sa->sa.sa_family);
	}
	
	/* Port  / Interface */
	switch (sa->sa.sa_family) {
		case AF_INET6:
		case AF_INET:
			strap(buf, len, ":%hu", ntohs(sa->sin.sin_port));
			break;

		case AF_PACKET: {
			struct nl_cache *cache = nl_cache_mngt_require("route/link");
			struct rtnl_link *link = rtnl_link_get(cache, sa->sll.sll_ifindex);
			if (!link)
				error("Failed to get interface for index: %u", sa->sll.sll_ifindex);
			
			strap(buf, len, "%%%s", rtnl_link_get_name(link));
			strap(buf, len, ":%hu", ntohs(sa->sll.sll_protocol));
			break;
		}
	}

	return 0;
}

int socket_parse_addr(const char *addr, struct sockaddr *saddr, enum socket_layer layer, int flags)
{
	/** @todo: Add support for IPv6 */
	union sockaddr_union *sa = (union sockaddr_union *) saddr;

	char *copy = strdup(addr);
	int ret;

	if (layer == LAYER_ETH) { /* Format: "ab:cd:ef:12:34:56%ifname:protocol" */
		/* Split string */
		char *node = strtok(copy, "%");
		char *ifname = strtok(NULL, ":");
		char *proto = strtok(NULL, "\0");

		/* Parse link layer (MAC) address */
		struct ether_addr *mac = ether_aton(node);
		if (!mac)
			error("Failed to parse MAC address: %s", node);

		memcpy(&sa->sll.sll_addr, &mac->ether_addr_octet, 6);
		
		/* Get interface index from name */
		struct nl_cache *cache = nl_cache_mngt_require("route/link");
		struct rtnl_link *link = rtnl_link_get_by_name(cache, ifname);
		if (!link)
			error("Failed to get network interface: '%s'", ifname);

		sa->sll.sll_protocol = htons((proto) ? strtol(proto, NULL, 0) : ETH_P_S2SS);
		sa->sll.sll_halen = 6;
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
			case LAYER_IP:
				hint.ai_socktype = SOCK_RAW;
				hint.ai_protocol = (service) ? strtol(service, NULL, 0) : IPPROTO_S2SS;
				hint.ai_flags |= AI_NUMERICSERV;
				break;

			case LAYER_UDP:
				hint.ai_socktype = SOCK_DGRAM;
				hint.ai_protocol = IPPROTO_UDP;
				break;

			default:
				error("Invalid address type");
		}

		/* Lookup address */
		struct addrinfo *result;
		ret = getaddrinfo(node, (layer == LAYER_IP) ? NULL : service, &hint, &result);
		if (!ret) {

			if (layer == LAYER_IP) {
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
