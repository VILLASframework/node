/** Interface related functions
 *
 * These functions are used to manage a network interface.
 * Most of them make use of Linux-specific APIs.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <list>

#include <sys/types.h>
#include <sys/socket.h>

#include <villas/log.hpp>

#ifndef SO_MARK
  #define SO_MARK	36	/**< Workaround: add missing constant for OPAL-RT Redhawk target */
#endif

/* Forward declarations */
struct nl_addr;
struct rtnl_link;
struct rtnl_qdisc;

namespace villas {
namespace node {

/* Forward declarations */
class Node;
class SuperNode;

}

namespace kernel {

/** Interface data structure */
class Interface {

public:
	struct rtnl_link *nl_link;		/**< libnl3: Handle of interface. */
	struct rtnl_qdisc *tc_qdisc;		/**< libnl3: Root priority queuing discipline (qdisc). */

protected:
	int affinity;				/**< IRQ / Core Affinity of this interface. */

	std::list<int> irqs;			/**< List of IRQs of the NIC. */
	std::list<node::Node *> nodes;	/**< List of nodes which use this interface. */

	Logger logger;

public:
	/** Add a new interface to the global list and lookup name, irqs... */
	Interface(struct rtnl_link *link, int affinity = 0);
	~Interface();

	/** Start interface.
	 *
	 * This setups traffic controls queue discs, network emulation and
	 * maps interface IRQs according to affinity.
	 *
	 * @param i A pointer to the interface structure.
	 * @retval 0 Success. Everything went well.
	 * @retval <0 Error. Something went wrong.
	 */
	int start();

	/** Stop interface
	 *
	 * This resets traffic qdiscs ant network emulation
	 * and maps interface IRQs to all CPUs.
	 *
	 * @param i A pointer to the interface structure.
	 * @retval 0 Success. Everything went well.
	 * @retval <0 Error. Something went wrong.
	 */
	int stop();

	/** Find existing or create new interface instance on which packets for a certain destination
	 *  will leave the system.
	 */
	static
	Interface *
	getEgress(struct sockaddr *sa, node::SuperNode *sn);

	/** Get all IRQs for this interface.
	 *
	 * Only MSI IRQs are determined by looking at:
	 *  /sys/class/net/{ifname}/device/msi_irqs/
	 *
	 * @param i A pointer to the interface structure
	 * @retval 0 Success. Everything went well.
	 * @retval <0 Error. Something went wrong.
	 */
	int getIRQs();

	/** Change the SMP affinity of NIC interrupts.
	 *
	 * @param i A pointer to the interface structure
	 * @param affinity A mask specifying which cores should handle this interrupt.
	 * @retval 0 Success. Everything went well.
	 * @retval <0 Error. Something went wrong.
	 */
	int setAffinity(int affinity);

	std::string getName() const;

	void addNode(node::Node *n)
	{
		nodes.push_back(n);
	}
};

} /* namespace kernel */
} /* namespace villas */
