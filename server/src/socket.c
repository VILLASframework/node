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

#include <linux/if_packet.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "config.h"
#include "utils.h"
#include "socket.h"
#include "if.h"

int socket_print(struct node *n, char *buf, int len)
{
	struct socket *s = n->socket;

	char local[INET6_ADDRSTRLEN + 16];
	char remote[INET6_ADDRSTRLEN + 16];
	
	socket_print_addr(local, sizeof(local), (struct sockaddr*) &s->local);
	socket_print_addr(remote, sizeof(remote), (struct sockaddr*) &s->remote);

	return snprintf(buf, len, "local=%s, remote=%s", local, remote);
}

int socket_open(struct node *n)
{
	struct socket *s = n->socket;
	struct sockaddr_in *sin = (struct sockaddr_in *) &s->local;
	struct sockaddr_ll *sll = (struct sockaddr_ll *) &s->local;
	int ret;
	
	s->sd = s->sd2 = -1;
	
	/* Create socket */
	switch (node_type(n)) {
		case TCPD:
		case TCP:	s->sd = socket(sin->sin_family, SOCK_STREAM,	IPPROTO_TCP); break;
		case UDP:	s->sd = socket(sin->sin_family, SOCK_DGRAM,	IPPROTO_UDP); break;
		case IP:	s->sd = socket(sin->sin_family, SOCK_RAW,	ntohs(sin->sin_port)); break;
		case IEEE_802_3:s->sd = socket(sin->sin_family, SOCK_DGRAM,	sll->sll_protocol); break;
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
	if (node_type(n) == TCPD) {
		/* Listening TCP sockets will be connected later by calling accept() */
		s->sd2 = s->sd;
	}
	else if (node_type(n) != IEEE_802_3) {
		ret = connect(s->sd, (struct sockaddr *) &s->remote, sizeof(s->remote));
		if (ret < 0)
			serror("Failed to connect socket");
	}

	/* Determine outgoing interface */
	int index = if_getegress((struct sockaddr *) &s->remote);
	if (index < 0)
		error("Failed to get egress interface for node '%s'", n->name);

	struct interface *i = if_lookup_index(index);
	if (!i)
		i = if_create(index);

	list_add(i->sockets, s);
	i->refcnt++;

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

int socket_read(struct node* n, struct msg *m)
{
	/* Receive message from socket */
	int ret = recv(n->socket->sd, m, sizeof(struct msg), 0);
	if (ret == 0)
		error("Remote node '%s' closed the connection", n->name);
	else if (ret < 0)
		serror("Failed recv");

	/* Convert headers to host byte order */
	m->sequence = ntohs(m->sequence);

	/* Convert message to host endianess */
	if (m->endian != MSG_ENDIAN_HOST)
		msg_swap(m);

	debug(10, "Message received from node '%s': version=%u, type=%u, endian=%u, length=%u, sequence=%u",
		n->name, m->version, m->type, m->endian, m->length, m->sequence);

	return 0;
}

int socket_write(struct node* n, struct msg *m)
{
	struct socket *s = n->socket;
	int ret;

	/* Convert headers to network byte order */
	m->sequence = htons(m->sequence);

	if (node_type(n) == IEEE_802_3)
		ret = sendto(s->sd, m, MSG_LEN(m->length), 0, (struct sockaddr *) &s->remote, sizeof(s->remote));
	else
		ret = send(s->sd, m, MSG_LEN(m->length), 0);

	if (ret < 0)
		serror("Failed sendto");

	debug(10, "Message sent to node '%s': version=%u, type=%u, endian=%u, length=%u, sequence=%u",
		n->name, m->version, m->type, m->endian, m->length, ntohs(m->sequence));

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
			error("Unsupported address family: %u", sa->sa_family);
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
			.ai_family = AF_UNSPEC
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
