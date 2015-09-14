/** Netlink related functions.
 *
 * S2SS uses libnl3 to talk to the Linux kernel to gather networking related information
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdio.h>

#include <netlink/route/route.h>
#include <netlink/route/link.h>

#include "utils.h"
#include "nl.h"

/** Singleton for global netlink socket */
static struct nl_sock *sock = NULL;

struct nl_sock * nl_init()
{
	int ret;

	if (!sock) {
		/* Create connection to netlink */
		sock = nl_socket_alloc();
		if (!sock)
			error("Failed to allocate memory");

		if ((ret = nl_connect(sock, NETLINK_ROUTE)))
			error("Failed to connect to kernel: %s", nl_geterror(ret));
		
		/* Fill some caches */
		struct nl_cache *cache;
		if ((ret = rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache)))
			error("Failed to get list of interfaces: %s", nl_geterror(ret));

		nl_cache_mngt_provide(cache);
	}
	
	return sock;
}

void nl_shutdown()
{
	nl_close(sock);
	nl_socket_free(sock);
	
	sock = NULL;
}

static int egress_cb(struct nl_msg *msg, void *arg)
{
	struct rtnl_route **route = (struct rtnl_route **) arg;

	if (rtnl_route_parse(nlmsg_hdr(msg), route))
		return NL_SKIP;
	
	return NL_STOP;
}

int nl_get_egress(struct nl_addr *addr)
{
	int ret;
	struct nl_sock *sock = nl_init();
	struct nl_msg *msg = nlmsg_alloc_simple(RTM_GETROUTE, 0);
	
	/* Build message */	
	struct rtmsg rmsg = {
		.rtm_family = nl_addr_get_family(addr),
		.rtm_dst_len = nl_addr_get_prefixlen(addr),
	};
		
	if ((ret = nlmsg_append(msg, &rmsg, sizeof(rmsg), NLMSG_ALIGNTO)))
		return ret;
	if ((ret = nla_put_addr(msg, RTA_DST, addr)))
		return ret;

	/* Send message */
	ret = nl_send_auto(sock, msg);
	nlmsg_free(msg);
	if (ret < 0)
		return ret;
	
	/* Hook into receive chain */
	struct rtnl_route *route = NULL;
	struct nl_cb *cb = nl_cb_alloc(NL_CB_VALID);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, egress_cb, &route);
	
	/* Receive message */
	nl_recvmsgs_report(sock, cb);
	nl_wait_for_ack(sock);
	nl_cb_put(cb);
	
	/* Check result */
	if (route && (1 <= rtnl_route_get_nnexthops(route))) {
		struct rtnl_nexthop *nh = rtnl_route_nexthop_n(route, 0);
		return rtnl_route_nh_get_ifindex(nh);
	}
	else
		return -1;
}
