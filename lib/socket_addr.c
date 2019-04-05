/** Various functions to work with socket addresses
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
 */

#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <villas/socket_addr.h>
#include <villas/utils.h>
#include <villas/kernel/nl.h>

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

#ifdef WITH_SOCKET_LAYER_ETH
		case AF_PACKET:
			strcatf(&buf, "%02x", sa->sll.sll_addr[0]);
			for (int i = 1; i < sa->sll.sll_halen; i++)
				strcatf(&buf, ":%02x", sa->sll.sll_addr[i]);
			break;
#endif /* WITH_SOCKET_LAYER_ETH */
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

#ifdef WITH_SOCKET_LAYER_ETH
		case AF_PACKET: {
			struct nl_cache *cache = nl_cache_mngt_require("route/link");
			struct rtnl_link *link = rtnl_link_get(cache, sa->sll.sll_ifindex);
			if (!link)
				error("Failed to get interface for index: %u", sa->sll.sll_ifindex);

			strcatf(&buf, "%%%s", rtnl_link_get_name(link));
			strcatf(&buf, ":%hu", ntohs(sa->sll.sll_protocol));
			break;
		}
#endif /* WITH_SOCKET_LAYER_ETH */
	}

	return buf;
}

int socket_parse_address(const char *addr, struct sockaddr *saddr, enum socket_layer layer, int flags)
{
	/** @todo: Add support for IPv6 */
	union sockaddr_union *sa = (union sockaddr_union *) saddr;

	char *copy = strdup(addr);
	int ret;

	if (layer == SOCKET_LAYER_UNIX) { /* Format: "/path/to/socket" */
		sa->sun.sun_family = AF_UNIX;

		if (strlen(addr) > sizeof(sa->sun.sun_path) - 1)
			error("Length of unix socket path is too long!");

		memcpy(sa->sun.sun_path, addr, strlen(addr) + 1);

		ret = 0;
	}
#ifdef WITH_SOCKET_LAYER_ETH
	else if (layer == SOCKET_LAYER_ETH) { /* Format: "ab:cd:ef:12:34:56%ifname:protocol" */
		/* Split string */
		char *lasts;
		char *node = strtok_r(copy, "%", &lasts);
		char *ifname = strtok_r(NULL, ":", &lasts);
		char *proto = strtok_r(NULL, "\0", &lasts);

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
#endif /* WITH_SOCKET_LAYER_ETH */
	else {	/* Format: "192.168.0.10:12001" */
		struct addrinfo hint = {
			.ai_flags = flags,
			.ai_family = AF_UNSPEC
		};

		/* Split string */
		char *lasts;
		char *node = strtok_r(copy, ":", &lasts);
		char *service = strtok_r(NULL, "\0", &lasts);

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

#ifdef WITH_SOCKET_LAYER_ETH
		case AF_PACKET:
			CMP(ntohs(xu->sll.sll_protocol), ntohs(yu->sll.sll_protocol));
			CMP(xu->sll.sll_ifindex, yu->sll.sll_ifindex);
//			CMP(xu->sll.sll_pkttype, yu->sll.sll_pkttype);
//			CMP(xu->sll.sll_hatype, yu->sll.sll_hatype);

			CMP(xu->sll.sll_halen, yu->sll.sll_halen);
			return memcmp(xu->sll.sll_addr, yu->sll.sll_addr, xu->sll.sll_halen);
#endif /* WITH_SOCKET_LAYER_ETH */

		default:
			return -1;
	}

#undef CMP
}
