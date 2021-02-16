/** Setup network emulation
 *
 * We use the firewall mark to apply individual netem qdiscs
 * per node. Every node uses an own BSD socket.
 * By using so SO_MARK socket option (see socket(7))
 * we can classify traffic originating from a node seperately.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

/** @addtogroup kernel Kernel
 * @{
 */

#pragma once

#include <cstdint>

#include <netlink/route/qdisc.h>
#include <netlink/route/classifier.h>

#include <jansson.h>

namespace villas {
namespace kernel {

/* Forward declarations */
class Interface;

namespace tc {

typedef uint32_t tc_hdl_t;

/** Parse network emulator (netem) settings.
 *
 * @param json A jansson object containing the settings.
 * @param[out] ne A pointer to a libnl3 qdisc object where setting will be written to.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int netem_parse(struct rtnl_qdisc **ne, json_t *json);

/** Print network emulator (netem) setting into buffer.
 *
 * @param tc A pointer to the libnl3 qdisc object where settings will be read from.
 * @return A pointer to a string which must be freed() by the caller.
 */
char * netem_print(struct rtnl_qdisc *ne);

/** Add a new network emulator (netem) discipline.
 *
 * @param i[in] The interface to which this qdisc will be added.
 * @param qd[in,out] The libnl3 object of the new prio qdisc.
 * @param handle[in] The handle of the new qdisc.
 * @param parent[in] Make this qdisc a child of this class
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int netem(Interface *i, struct rtnl_qdisc **qd, tc_hdl_t handle, tc_hdl_t parent);

int netem_set_delay_distribution(struct rtnl_qdisc *qdisc, json_t *json);

} /* namespace tc */
} /* namespace kernel */
} /* namespace villas */

/** @} */
