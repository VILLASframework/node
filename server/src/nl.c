/** Netlink related functions
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

#include "utils.h"
#include "nl.h"

/** Singleton for netlink socket */
static struct nl_sock *sock = NULL;

struct nl_sock * nl_init()
{
	if (!sock) {
		/* Create connection to netlink */
		sock = nl_socket_alloc();
		if (!sock)
			error("Failed to allocate memory");

		nl_connect(sock, NETLINK_ROUTE);
	}
	
	return sock;
}

void nl_shutdown()
{
	nl_close(sock);
	nl_socket_free(sock);
	
	sock = NULL;
}

struct rtnl_link * nl_get_link(int ifindex, const char *dev)
{
	struct nl_sock *sock = nl_init();
	struct rtnl_link *link;

	if (rtnl_link_get_kernel(sock, ifindex, dev, &link))
		return NULL;
	else
		return link;
}

int nl_get_nexthop(struct nl_addr *addr, struct rtnl_nexthop **nexthop)
{
	int ret;

	struct rtnl_route *route;
	struct rtnl_nexthop *nh;

	struct nl_sock *sock = nl_init();
	struct nl_msg *msg = nlmsg_alloc_simple(RTM_GETROUTE, 0);
	
	struct nl_cache_ops *ops = nl_cache_ops_lookup_safe("route/route");
	struct nl_cache *cache = nl_cache_alloc(ops);
	
	struct rtmsg rmsg = {
		.rtm_family = nl_addr_get_family(addr),
		.rtm_dst_len = nl_addr_get_prefixlen(addr),
	};
	
	if ((ret = nlmsg_append(msg, &rmsg, sizeof(rmsg), NLMSG_ALIGNTO)) < 0)
		return ret;
	if ((ret = nla_put_addr(msg, RTA_DST, addr)) < 0)
		return ret;

	if ((ret = nl_send_auto_complete(sock, msg)) < 0)
		return ret;
	if ((ret = nl_cache_pickup(sock, cache)) < 0)
		return ret;

	if (!(route = (struct rtnl_route *) nl_cache_get_first(cache)))
		return -1;
	if (!(nh = rtnl_route_nexthop_n(route, 0)))
		return -1;
	
	*nexthop = rtnl_route_nh_clone(nh);

	nl_cache_put(cache);	
	nl_cache_ops_put(ops);
	nlmsg_free(msg);
	
	return 0;
}
