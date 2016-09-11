/** Various socket related functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>

#include <netinet/ether.h>
#include <netinet/ip.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>

#include "socket.h"
#include "config.h"
#include "utils.h"

#include "if.h"
#include "nl.h"
#include "tc.h"
#include "msg.h"
#include "sample.h"
#include "queue.h"

/* Forward declartions */
static struct node_type vt;

/* Private static storage */
struct list interfaces;

int socket_init(int argc, char * argv[], config_setting_t *cfg)
{
	if (getuid() != 0)
		error("The 'socket' node-type requires superuser privileges!");

	nl_init(); /* Fill link cache */
	list_init(&interfaces);

	/* Gather list of used network interfaces */
	list_foreach(struct node *n, &vt.instances) {
		struct socket *s = n->_vd;
		struct rtnl_link *link;

		/* Determine outgoing interface */
		if (if_get_egress((struct sockaddr *) &s->remote, &link)) {
			char *buf = socket_print_addr((struct sockaddr *) &s->remote);
			error("Failed to get interface for socket address '%s'", buf);
			free(buf);
		}

		/* Search of existing interface with correct ifindex */
		struct interface *i;
		list_foreach(i, &interfaces) {
			if (rtnl_link_get_ifindex(i->nl_link) == rtnl_link_get_ifindex(link))
				goto found;
		}

		/* If not found, create a new interface */
		i = if_create(link);
		list_push(&interfaces, i);

found:		list_push(&i->sockets, s);
	}

	/** @todo Improve mapping of NIC IRQs per path */
	int affinity;
	if (!config_setting_lookup_int(cfg, "affinity", &affinity))
		affinity = -1;

	list_foreach(struct interface *i, &interfaces)
		if_start(i, affinity);

	return 0;
}

int socket_deinit()
{
	list_foreach(struct interface *i, &interfaces)
		if_stop(i);

	list_destroy(&interfaces, (dtor_cb_t) if_destroy, false);

	return 0;
}

char * socket_print(struct node *n)
{
	struct socket *s = n->_vd;
	char *layer = NULL, *header = NULL, *buf;

	switch (s->layer) {
		case SOCKET_LAYER_UDP:	layer = "udp";	break;
		case SOCKET_LAYER_IP:	layer = "ip";	break;
		case SOCKET_LAYER_ETH:	layer = "eth";	break;
	}

	switch (s->header) {
		case SOCKET_HEADER_GTNET_SKT:	header = "gtnet-skt";	break;
		case SOCKET_HEADER_DEFAULT:	header = "villas";	break;
	}

	char *local = socket_print_addr((struct sockaddr *) &s->local);
	char *remote = socket_print_addr((struct sockaddr *) &s->remote);

	buf = strf("layer=%s, header=%s, local=%s, remote=%s", layer, header, local, remote);

	free(local);
	free(remote);

	return buf;
}

int socket_open(struct node *n)
{
	struct socket *s = n->_vd;
	struct sockaddr_in *sin = (struct sockaddr_in *) &s->local;
	struct sockaddr_ll *sll = (struct sockaddr_ll *) &s->local;
	int ret;

	/* Create socket */
	switch (s->layer) {
		case SOCKET_LAYER_UDP:	s->sd = socket(sin->sin_family, SOCK_DGRAM,	IPPROTO_UDP); break;
		case SOCKET_LAYER_IP:	s->sd = socket(sin->sin_family, SOCK_RAW,	ntohs(sin->sin_port)); break;
		case SOCKET_LAYER_ETH:	s->sd = socket(sll->sll_family, SOCK_DGRAM,	sll->sll_protocol); break;
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
			debug(DBG_SOCKET | 4, "Set FW mark for socket (sd=%u) to %u", s->sd, s->mark);
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
				debug(DBG_SOCKET | 4, "Set QoS/TOS IP option for node %s to %#x", node_name(n), prio);
			break;

		default:
			prio = SOCKET_PRIO;
			if (setsockopt(s->sd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
				serror("Failed to set socket priority");
			else
				debug(DBG_SOCKET | 4, "Set socket priority for node %s to %d", node_name(n), prio);
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

int socket_close(struct node *n)
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

int socket_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct socket *s = n->_vd;

	int samples, ret, received, length;
	ssize_t bytes;

	if (s->header == SOCKET_HEADER_GTNET_SKT) {
		if (cnt < 1)
			return 0;

		/* The GTNETv2-SKT protocol send every sample in a single packet.
		 * socket_read() receives a single packet. */
		struct sample *smp = smps[0];

		/* Receive next sample */
		bytes = recv(s->sd, &smp->values[0], SAMPLE_DATA_LEN(smp->capacity), MSG_TRUNC);
		if (bytes == 0)
			error("Remote node %s closed the connection", node_name(n)); /** @todo Should we really hard fail here? */
		else if (bytes < 0)
			serror("Failed recv from node %s", node_name(n));
		else if (bytes % 4 != 0) {
			warn("Packet size is invalid: %zd Must be multiple of 4 bytes.", bytes);
			recv(s->sd, NULL, 0, 0); /* empty receive buffer */
			return -1;
		}

		debug(3, "Received %zd bytes", bytes);

		length = bytes / SAMPLE_DATA_LEN(1);
		if (length > smp->capacity) {
			warn("Node %s received more values than supported. Dropping %u values", node_name(n), length - smp->capacity);
			length = smp->capacity;
		}

		/** @todo Should we generate sequence no here manually?
		 *        Or maybe optinally use the first data value as a sequence?
		 *        However this would require the RTDS model to be changed. */
		smp->sequence = 0;
		smp->length = length;

		received = 1; /* GTNET-SKT sends every sample in a single packet */
	}
	else {
		struct msg msgs[cnt];
		struct msg hdr;
		struct iovec iov[2*cnt];
		struct msghdr mhdr = {
			.msg_iov = iov
		};

		/* Peak into message header of the first sample and to get total packet size. */
		bytes = recv(s->sd, &hdr, sizeof(struct msg), MSG_PEEK | MSG_TRUNC);
		if (bytes < sizeof(struct msg) || bytes % 4 != 0) {
			warn("Packet size is invalid: %zd Must be multiple of 4 bytes.", bytes);
			recv(s->sd, NULL, 0, 0); /* empty receive buffer */
			return -1;
		}

		ret = msg_verify(&hdr);
		if (ret) {
			warn("Invalid message received: reason=%d, bytes=%zd", ret, bytes);
			recv(s->sd, NULL, 0, 0); /* empty receive buffer */
			return -1;
		}

		/* Convert message to host endianess */
		if (hdr.endian != MSG_ENDIAN_HOST)
			msg_swap(&hdr);

		samples = bytes / MSG_LEN(hdr.values);
		if (samples > cnt) {
			warn("Node %s received more samples than supported. Dropping %u samples", node_name(n), samples - cnt);
			samples = cnt;
		}

		/* We expect that all received samples have the same amount of values! */
		for (int i = 0; i < samples; i++) {
			iov[2*i+0].iov_base = &msgs[i];
			iov[2*i+0].iov_len = MSG_LEN(0);

			iov[2*i+1].iov_base = SAMPLE_DATA_OFFSET(smps[i]);
			iov[2*i+1].iov_len  = SAMPLE_DATA_LEN(hdr.values);

			mhdr.msg_iovlen += 2;

			if (hdr.values > smps[i]->capacity)
				error("Node %s received more values than supported. Dropping %d values.", node_name(n), hdr.values - smps[i]->capacity);
		}

		/* Receive message from socket */
		bytes = recvmsg(s->sd, &mhdr, 0);	//--? samples - cnt samples dropped
		if (bytes == 0)
			error("Remote node %s closed the connection", node_name(n));
		else if (bytes < 0)
			serror("Failed recv from node %s", node_name(n));

		for (received = 0; received < samples; received++) {
			struct msg *m = &msgs[received];
			struct sample *smp = smps[received];

			ret = msg_verify(m);
			if (ret)
				break;

			if (m->values != hdr.values)
				break;

			/* Convert message to host endianess */
			if (m->endian != MSG_ENDIAN_HOST)
				msg_swap(m);

			smp->length = m->values;
			smp->sequence = m->sequence;
			smp->ts.origin = MSG_TS(m);
		}
	}

	debug(DBG_SOCKET | 17, "Received message of %zd bytes: %u samples", bytes, received);

	return received;
}

int socket_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct socket *s = n->_vd;
	ssize_t bytes;
	int sent = 0;

	/* Construct iovecs */
	if (s->header == SOCKET_HEADER_GTNET_SKT) {
		if (cnt < 1)
			return 0;

		for (int i = 0; i < cnt; i++) {
			bytes = sendto(s->sd, &smps[i]->values, SAMPLE_DATA_LEN(smps[i]->length), 0, (struct sockaddr *) &s->remote, sizeof(s->remote));
			if (bytes < 0)
				serror("Failed send to node %s", node_name(n));

			sent++;

			debug(DBG_SOCKET | 17, "Sent packet of %zd bytes with 1 sample", bytes);
		}
	}
	else {
		struct msg msgs[cnt];
		struct iovec iov[2*cnt];
		struct msghdr mhdr = {
			.msg_iov = iov,
			.msg_iovlen = ARRAY_LEN(iov),
			.msg_name = (struct sockaddr *) &s->remote,
			.msg_namelen = sizeof(s->remote)
		};

		for (int i = 0; i < cnt; i++) {

			msgs[i] = MSG_INIT(smps[i]->length, smps[i]->sequence);

			msgs[i].ts.sec  = smps[i]->ts.origin.tv_sec;
			msgs[i].ts.nsec = smps[i]->ts.origin.tv_nsec;

			iov[i*2+0].iov_base = &msgs[i];
			iov[i*2+0].iov_len  = MSG_LEN(0);

			iov[i*2+1].iov_base = SAMPLE_DATA_OFFSET(smps[i]);
			iov[i*2+1].iov_len  = SAMPLE_DATA_LEN(smps[i]->length);
		}

		/* Send message */
		bytes = sendmsg(s->sd, &mhdr, 0);
		if (bytes < 0)
			serror("Failed send to node %s", node_name(n));

		sent = cnt; /** @todo Find better way to determine how many values we actually sent */

		debug(DBG_SOCKET | 17, "Sent packet of %zd bytes with %u samples", bytes, cnt);
	}

	return sent;
}

int socket_parse(struct node *n, config_setting_t *cfg)
{
	const char *local, *remote, *layer, *hdr;
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
		if (!strcmp(hdr, "gtnet-skt"))
			s->header = SOCKET_HEADER_GTNET_SKT;
		else if (!strcmp(hdr, "default"))
			s->header = SOCKET_HEADER_DEFAULT;
		else
			cerror(cfg, "Invalid application header type '%s' for node %s", hdr, node_name(n));
	}

	if (!config_setting_lookup_string(cfg, "remote", &remote))
		cerror(cfg, "Missing remote address for node %s", node_name(n));

	if (!config_setting_lookup_string(cfg, "local", &local))
		cerror(cfg, "Missing local address for node %s", node_name(n));

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

	config_setting_t *cfg_netem = config_setting_get_member(cfg, "netem");
	if (cfg_netem) {
		int enabled = 1;
		if (!config_setting_lookup_bool(cfg_netem, "enabled", &enabled) || enabled)
			tc_parse(cfg_netem, &s->tc_qdisc);
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

		memcpy(&sa->sll.sll_addr, &mac->ether_addr_octet, 6);

		/* Get interface index from name */
		struct nl_cache *cache = nl_cache_mngt_require("route/link");
		struct rtnl_link *link = rtnl_link_get_by_name(cache, ifname);
		if (!link)
			error("Failed to get network interface: '%s'", ifname);

		sa->sll.sll_protocol = htons((proto) ? strtol(proto, NULL, 0) : ETH_P_VILLAS);
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

static struct node_type vt = {
	.name		= "socket",
	.description	= "BSD network sockets",
	.vectorize	= 0,
	.size		= sizeof(struct socket),
	.destroy	= socket_destroy,
	.reverse	= socket_reverse,
	.parse		= socket_parse,
	.print		= socket_print,
	.open		= socket_open,
	.close		= socket_close,
	.read		= socket_read,
	.write		= socket_write,
	.init		= socket_init,
	.deinit		= socket_deinit
};

REGISTER_NODE_TYPE(&vt)
