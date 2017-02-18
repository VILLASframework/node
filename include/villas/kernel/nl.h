/** Netlink related functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

#include <netlink/netlink.h>
#include <netlink/route/route.h>
#include <netlink/route/link.h>

/** Get index of outgoing interface for given destination address.
 *
 * @retval >=0 Interface index of outgoing interface.
 * @retval <0 Error. Something went wrong.
 */
int nl_get_egress(struct nl_addr *addr);	

/** Get or create global netlink socket. */
struct nl_sock * nl_init();

/** Close and free global netlink socket. */
void nl_shutdown();