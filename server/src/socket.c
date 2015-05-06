/** Various socket related functions
 *
 * Parse and print addresses, connect, close, etc...
 *
 * S2SS uses these functions to setup the network emulation feature.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <linux/if_packet.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>

#include "config.h"
#include "utils.h"
#include "socket.h"
#include "if.h"

/** Linked list of interfaces */
extern struct list interfaces;

/** Linked list of sockets */
struct list sockets;

int socket_init(int argc, char * argv[], struct settings *set)
{ INDENT
	list_init(&interfaces, (dtor_cb_t) if_destroy);
	
	/* Gather list of used network interfaces */
	FOREACH(&sockets, it) {
		struct socket *s = it->socket;

		/* Determine outgoing interface */
		int index = if_getegress((struct sockaddr *) &s->remote);
		if (index < 0) {
			char buf[128];
			socket_print_addr(buf, sizeof(buf), (struct sockaddr *) &s->remote);
			error("Failed to get interface for socket address '%s'", buf);
		}

		struct interface *i = if_lookup_index(index);
		if (!i)
			i = if_create(index);

		list_push(&i->sockets, s);
		i->refcnt++;
	}
	
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
	
	socket_print_addr(local, sizeof(local), (struct sockaddr *) &s->local);
	socket_print_addr(remote, sizeof(remote), (struct sockaddr *) &s->remote);

	return snprintf(buf, len, "local=%s, remote=%s", local, remote);
}

int socket_open(struct node *n)
{
	struct socket *s = n->socket;
	struct sockaddr_in *sin = (struct sockaddr_in *) &s->local;
	struct sockaddr_ll *sll = (struct sockaddr_ll *) &s->local;
	int ret;
		
	/* Create socket */
	switch (node_type(n)) {
		case TCPD:
		case TCP:	s->sd = socket(sin->sin_family, SOCK_STREAM,	IPPROTO_TCP); break;
		case UDP:	s->sd = socket(sin->sin_family, SOCK_DGRAM,	IPPROTO_UDP); break;
		case IP:	s->sd = socket(sin->sin_family, SOCK_RAW,	ntohs(sin->sin_port)); break;
		case IEEE_802_3:s->sd = socket(sll->sll_family, SOCK_DGRAM,	sll->sll_protocol); break;
		default:
			error("Invalid socket type!");
	}
	
	if (s->sd < 0)
		serror("Failed to create socket");
	
	/* Bind socket for receiving */
	ret = bind(s->sd, (struct sockaddr *) &s->local, sizeof(s->local));
	if (ret < 0)
		serror("Failed to bind socket");
	
	/* Connect socket for sending */
	if (node_type(n) == TCP) {
		s->sd2 = s->sd;
		ret = connect(s->sd, (struct sockaddr *) &s->remote, sizeof(s->remote));
		if (ret < 0)
			serror("Failed to connect socket");
	}

	/* Set socket priority, QoS or TOS IP options */
	int prio;
	switch (node_type(n)) {
		case TCPD:
		case TCP:
		case UDP:
		case IP:
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
	
	if (s->sd2 >= 0)
		close(s->sd2);
	
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
	
	/* Check packet integrity */
	if (bytes % (cnt * 4) != 0)
		error("Packet length not dividable by 4!");
	if (bytes / cnt > sizeof(struct msg))
		error("Packet length is too large!");
	
	for (int i = 0; i < cnt; i++) {
		/* All messages of a packet must have equal length! */
		iov[i].iov_base = &pool[(first+poolsize-i) % poolsize];
		iov[i].iov_len  = bytes / cnt;
	}
	
	/* Receive message from socket */
	int ret = recvmsg(s->sd, &mhdr, 0);
	if (ret == 0)
		error("Remote node '%s' closed the connection", n->name);
	else if (ret < 0)
		serror("Failed recv");

	for (int i = 0; i < cnt; i++) {
		struct msg *n = &pool[(first+poolsize-i) % poolsize];

		/* Check integrity of packet */
		bytes -= MSG_LEN(n->length);
		
		/* Convert headers to host byte order */
		n->sequence = ntohs(n->sequence);
		
		/* Convert message to host endianess */
		if (n->endian != MSG_ENDIAN_HOST)
			msg_swap(n);
	}

	/* Check packet integrity */
	if (bytes != 0)
		error("Packet length does not match message header length!");

	return cnt;
}

int socket_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct socket *s = n->socket;
	int ret = -1;
	
	struct iovec iov[cnt];
	struct msghdr mhdr = {
		.msg_iov = iov,
		.msg_iovlen = ARRAY_LEN(iov)
	};
	
	for (int i = 0; i < cnt; i++) {
		struct msg *n = &pool[(first+poolsize+i) % poolsize];
		
		/* Convert headers to host byte order */
		n->sequence = htons(n->sequence);
		
		iov[i].iov_base = n;
		iov[i].iov_len  = MSG_LEN(n->length);
	}

	/* Specify destination address for connection-less procotols */
	switch (node_type(n)) {
		case IEEE_802_3:
		case IP:
		case UDP:
			mhdr.msg_name = (struct sockaddr *) &s->remote;
			mhdr.msg_namelen = sizeof(s->remote);
			break;
		default:
			break;
	}
	
	ret = sendmsg(s->sd, &mhdr, 0);
	if (ret < 0)
		serror("Failed send");

	return cnt;
}

int socket_parse(config_setting_t *cfg, struct node *n)
{
	const char *local, *remote;
	int ret;
	
	struct socket *s = alloc(sizeof(struct socket));

	if (!config_setting_lookup_string(cfg, "remote", &remote))
		cerror(cfg, "Missing remote address for node '%s'", n->name);

	if (!config_setting_lookup_string(cfg, "local", &local))
		cerror(cfg, "Missing local address for node '%s'", n->name);

	ret = socket_parse_addr(local, (struct sockaddr *) &s->local, node_type(n), AI_PASSIVE);
	if (ret)
		cerror(cfg, "Failed to resolve local address '%s' of node '%s': %s",
			local, n->name, gai_strerror(ret));

	ret = socket_parse_addr(remote, (struct sockaddr *) &s->remote, node_type(n), 0);
	if (ret)
		cerror(cfg, "Failed to resolve remote address '%s' of node '%s': %s",
			remote, n->name, gai_strerror(ret));

	/** @todo Netem settings are not usable AF_UNIX */
	config_setting_t *cfg_netem = config_setting_get_member(cfg, "netem");
	if (cfg_netem) {
		s->netem = alloc(sizeof(struct netem));
			
		tc_parse(cfg_netem, s->netem);
	}
	
	n->socket = s;
	
	list_push(&sockets, s);

	return 0;
}

int socket_print_addr(char *buf, int len, struct sockaddr *sa)
{
	switch (sa->sa_family) {
		case AF_INET: {
			struct sockaddr_in *sin = (struct sockaddr_in *) sa;
			inet_ntop(sin->sin_family, &sin->sin_addr, buf, len);
			return snprintf(buf+strlen(buf), len-strlen(buf), ":%hu", ntohs(sin->sin_port));
		}

		case AF_PACKET: {
			struct sockaddr_ll *sll = (struct sockaddr_ll *) sa;
			char ifname[IF_NAMESIZE];

			return snprintf(buf, len, "%s%%%s:%#hx", 
				ether_ntoa((struct ether_addr *) &sll->sll_addr),
				if_indextoname(sll->sll_ifindex, ifname),
				ntohs(sll->sll_protocol));
		}

		default:
			return snprintf(buf, len, "address family: %u", sa->sa_family);
	}
	
	return 0;
}

int socket_parse_addr(const char *addr, struct sockaddr *sa, enum node_type type, int flags)
{
	/** @todo: Add support for IPv6 */

	char *copy = strdup(addr);
	int ret;
	
	if (type == IEEE_802_3) { /* Format: "ab:cd:ef:12:34:56%ifname:protocol" */
		struct sockaddr_ll *sll = (struct sockaddr_ll *) sa;

		/* Split string */
		char *node = strtok(copy, "%");
		char *ifname = strtok(NULL, ":");
		char *proto = strtok(NULL, "\0");

		/* Parse link layer (MAC) address */
		struct ether_addr *mac = ether_aton(node);
		if (!mac)
			error("Failed to parse mac address: %s", node);

		memcpy(&sll->sll_addr, &mac->ether_addr_octet, 6);

		sll->sll_protocol = htons((proto) ? strtol(proto, NULL, 0) : ETH_P_S2SS);
		sll->sll_halen = 6;
		sll->sll_family = AF_PACKET;
		sll->sll_ifindex = if_nametoindex(ifname);

		ret = 0;
	}
	else {	/* Format: "192.168.0.10:12001" */
		struct addrinfo hint = {
			.ai_flags = flags,
			.ai_family = AF_INET
		};
		
		/* Split string */
		char *node = strtok(copy, ":");
		char *service = strtok(NULL, "\0");

		if (node && !strcmp(node, "*"))
			node = NULL;

		if (service && !strcmp(service, "*"))
			service = NULL;

		switch (type) {
			case IP:
				hint.ai_socktype = SOCK_RAW;
				hint.ai_protocol = (service) ? strtol(service, NULL, 0) : IPPROTO_S2SS;
				hint.ai_flags |= AI_NUMERICSERV;
				break;

			case TCPD:
			case TCP:
				hint.ai_socktype = SOCK_STREAM;
				hint.ai_protocol = IPPROTO_TCP;
				break;

			case UDP:
				hint.ai_socktype = SOCK_DGRAM;
				hint.ai_protocol = IPPROTO_UDP;
				break;

			default:
				error("Invalid address type");
		}

		/* Lookup address */
		struct addrinfo *result;
		ret = getaddrinfo(node, (type == IP) ? NULL : service, &hint, &result);
		if (!ret) {
			
			if (type == IP) {
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
