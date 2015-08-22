/** 
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _NL_H_
#define _NL_H_

#include <netlink/netlink.h>
#include <netlink/route/route.h>
#include <netlink/route/link.h>

/** Get libnl3 link object from interface name
 *
 * @param dev The name of the interface
 * @retval 0 Error. Something went wrong.
 * @retval >0 A pointer to the libnl3 link object.
 */
struct rtnl_link * nl_get_link(int ifindex, const char *dev);

/**
 *
 */
int nl_get_nexthop(struct nl_addr *addr, struct rtnl_nexthop **nexthop);	

/**
 *
 */
struct nl_sock * nl_init();

/**
 *
 */
void nl_shutdown();

#endif