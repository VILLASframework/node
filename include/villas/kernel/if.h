/** Interface related functions
 *
 * These functions are used to manage a network interface.
 * Most of them make use of Linux-specific APIs.
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

#include <sys/types.h>
#include <sys/socket.h>

#include <villas/list.h>

#define IF_IRQ_MAX	3	/**< Maxmimal number of IRQs of an interface */

#ifndef SO_MARK
  #define SO_MARK	36	/**< Workaround: add missing constant for OPAL-RT Redhawk target */
#endif

struct socket;
struct nl_addr;
struct rtnl_link;

/** Interface data structure */
struct interface {
	struct rtnl_link *nl_link;	/**< libnl3: Handle of interface. */
	struct rtnl_qdisc *tc_qdisc;	/**< libnl3: Root priority queuing discipline (qdisc). */

	char irqs[IF_IRQ_MAX];		/**< List of IRQs of the NIC. */
	int affinity;			/**< IRQ / Core Affinity of this interface. */

	struct vlist nodes;		/**< Linked list of nodes which use this interface. */
};

/** Add a new interface to the global list and lookup name, irqs...
 *
 * @param link The libnl3 link handle
 * @retval >0 Success. A pointer to the new interface.
 * @retval 0 Error. The creation failed.
 */
int if_init(struct interface * , struct rtnl_link *link) __attribute__ ((warn_unused_result));

/** Get name of interface */
const char * if_name(struct interface *);

/** Destroy interface by freeing dynamically allocated memory.
 *
 * @param i A pointer to the interface structure.
 */
int if_destroy(struct interface *i) __attribute__ ((warn_unused_result));

/** Start interface.
 *
 * This setups traffic controls queue discs, network emulation and
 * maps interface IRQs according to affinity.
 *
 * @param i A pointer to the interface structure.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int if_start(struct interface *i);

/** Stop interface
 *
 * This resets traffic qdiscs ant network emulation
 * and maps interface IRQs to all CPUs.
 *
 * @param i A pointer to the interface structure.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int if_stop(struct interface *i);

/** Find existing or create new interface instance on which packets for a certain destination
 *  will leave the system.
 */
struct interface * if_get_egress(struct sockaddr *sa, struct vlist *interfaces);

/** Lookup routing tables to get the interface on which packets for a certain destination
 *  will leave the system.
 *
 * @param[in] sa The destination address for outgoing packets.
 * @param[out] link The egress interface.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
struct rtnl_link * if_get_egress_link(struct sockaddr *sa);

/** Get all IRQs for this interface.
 *
 * Only MSI IRQs are determined by looking at:
 *  /sys/class/net/{ifname}/device/msi_irqs/
 *
 * @param i A pointer to the interface structure
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int if_get_irqs(struct interface *i);

/** Change the SMP affinity of NIC interrupts.
 *
 * @param i A pointer to the interface structure
 * @param affinity A mask specifying which cores should handle this interrupt.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int if_set_affinity(struct interface *i, int affinity);

/** @} */
