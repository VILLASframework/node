/** Netlink related functions.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <sys/socket.h>

#include <netlink/netlink.h>
#include <netlink/route/route.h>
#include <netlink/route/link.h>

namespace villas {
namespace kernel {
namespace nl {

/** Get index of outgoing interface for given destination address.
 *
 * @retval >=0 Interface index of outgoing interface.
 * @retval <0 Error. Something went wrong.
 */
int get_egress(struct nl_addr *addr);

/** Lookup routing tables to get the interface on which packets for a certain destination
 *  will leave the system.
 *
 * @param[in] sa The destination address for outgoing packets.
 * @param[out] link The egress interface.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
struct rtnl_link * get_egress_link(struct sockaddr *sa);

/** Get or create global netlink socket. */
struct nl_sock * init();

/** Close and free global netlink socket. */
void shutdown();

} // namespace nl
} // namespace kernel
} // namespace villas
