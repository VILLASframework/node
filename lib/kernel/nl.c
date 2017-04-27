/** Netlink related functions.
 *
 * VILLASnode uses libnl3 to talk to the Linux kernel to gather networking related information
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

#include <stdio.h>

#include <netlink/route/route.h>
#include <netlink/route/link.h>

#include "kernel/nl.h"

#include "utils.h"

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

		ret = nl_connect(sock, NETLINK_ROUTE);
		if (ret)
			error("Failed to connect to kernel: %s", nl_geterror(ret));
		
		/* Fill some caches */
		struct nl_cache *cache;
		ret = rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache);
		if (ret)
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
	struct nl_cb *cb;
	struct nl_msg *msg = nlmsg_alloc_simple(RTM_GETROUTE, 0);
	struct rtnl_route *route = NULL;
	
	/* Build message */	
	struct rtmsg rmsg = {
		.rtm_family = nl_addr_get_family(addr),
		.rtm_dst_len = nl_addr_get_prefixlen(addr),
	};
	
	ret = nlmsg_append(msg, &rmsg, sizeof(rmsg), NLMSG_ALIGNTO);
	if (ret)
		goto out;

	ret = nla_put_addr(msg, RTA_DST, addr);
	if (ret)
		goto out;

	/* Send message */
	ret = nl_send_auto(sock, msg);
	if (ret < 0)
		goto out;
	
	/* Hook into receive chain */
	cb = nl_cb_alloc(NL_CB_CUSTOM);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, egress_cb, &route);
	
	/* Receive message */
	nl_recvmsgs_report(sock, cb);
	nl_wait_for_ack(sock);
	
	/* Check result */
	if (!route || rtnl_route_get_nnexthops(route) != 1) {
		ret = -1;
		goto out2;
	}
		
	ret = rtnl_route_nh_get_ifindex(rtnl_route_nexthop_n(route, 0));

	rtnl_route_put(route);

out2:	nl_cb_put(cb);	
out:	nlmsg_free(msg);

	return ret;
}
