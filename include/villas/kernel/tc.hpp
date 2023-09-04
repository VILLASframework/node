/* Setup interface queuing desciplines for network emulation.
 *
 * We use the firewall mark to apply individual netem qdiscs
 * per node. Every node uses an own BSD socket.
 * By using so SO_MARK socket option (see socket(7))
 * we can classify traffic originating from a node seperately.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>

#include <netlink/route/qdisc.h>
#include <netlink/route/classifier.h>

#include <jansson.h>

namespace villas {
namespace kernel {

// Forward declarations
class Interface;

namespace tc {

typedef uint32_t tc_hdl_t;

/* Remove all queuing disciplines and filters.
 *
 * @param i The interface
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int reset(Interface *i);

/* Create a priority (prio) queueing discipline.
 *
 * @param i[in] The interface
 * @param qd[in,out] The libnl3 object of the new prio qdisc.
 * @param handle[in] The handle for the new qdisc
 * @param parent[in] Make this qdisc a child of this class
 * @param bands[in] The number of classes for this new qdisc
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int prio(Interface *i, struct rtnl_qdisc **qd, tc_hdl_t handle, tc_hdl_t, int bands);

/* Add a new filter based on the netfilter mark.
 *
 * @param i The interface to which this classifier is applied to.
 * @param cls[in,out] The libnl3 object of the new prio qdisc.
 * @param flowid The destination class for matched traffic
 * @param mark The netfilter firewall mark (sometime called 'fwmark')
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int mark(Interface *i, struct rtnl_cls **cls, tc_hdl_t flowid, uint32_t mark);

} // namespace tc
} // namespace kernel
} // namespace villas
