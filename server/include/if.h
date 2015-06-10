/** Interface related functions
 *
 * These functions are used to manage a network interface.
 * Most of them make use of Linux-specific APIs.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _IF_H_
#define _IF_H_

#include <sys/types.h>
#include <net/if.h>

#include "list.h"

#define IF_NAME_MAX	IFNAMSIZ /**< Maximum length of an interface name */
#define IF_IRQ_MAX	3	 /**< Maxmimal number of IRQs of an interface */

#ifndef SO_MARK
 #define SO_MARK	36	/**< Workaround: add missing constant for OPAL-RT Redhawk target */
#endif

struct socket;

/** Interface data structure */
struct interface {
	/** The index used by the kernel to reference this interface */
	int index;
	/** Human readable name of this interface */
	char name[IF_NAME_MAX];
	/** List of IRQs of the NIC */
	char irqs[IF_IRQ_MAX];

	/** Linked list of associated sockets */
	struct list sockets;
};

/** Add a new interface to the global list and lookup name, irqs...
 *
 * @param index The interface index of the OS
 * @retval >0 Success. A pointer to the new interface.
 * @retval 0 Error. The creation failed.
 */
struct interface * if_create(int index);


/** Destroy interface by freeing dynamically allocated memory.
 *
 * @param i A pointer to the interface structure.
 */
void if_destroy(struct interface *i);

/** Start interface.
 *
 * This setups traffic controls queue discs, network emulation and
 * maps interface IRQs according to affinity.
 *
 * @param i A pointer to the interface structure.
 * @param affinity Set the IRQ affinity of this interface.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int if_start(struct interface *i, int affinity);

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

/** Get outgoing interface.
 *
 * Depending on the address family of the socker address,
 * this function tries to determine outgoing interface
 * which is used to send packages to a remote host with the specified
 * socket address.
 *
 * For AF_INET the fnuction performs a lookup in the kernel routing table.
 * For AF_PACKET the function uses the existing sll_ifindex field of the socket address.
 *
 * @param sa A destination address for outgoing packets.
 * @return The interface index.
 */
int if_getegress(struct sockaddr *sa);

/** Get all IRQs for this interface.
 *
 * Only MSI IRQs are determined by looking at:
 *  /sys/class/net/{ifname}/device/msi_irqs/
 *
 * @param i A pointer to the interface structure
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int if_getirqs(struct interface *i);

/** Change the SMP affinity of NIC interrupts.
 *
 * @param i A pointer to the interface structure
 * @param affinity A mask specifying which cores should handle this interrupt.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int if_setaffinity(struct interface *i, int affinity);

/** Search the global list of interfaces for a given index.
 *
 * @param index The interface index to search for
 * @param interfaces A linked list of all interfaces
 * @retval NULL if no interface with index was found.
 * @retval >0 Success. A pointer to the interface.
 */
struct interface * if_lookup_index(int index);

#endif /* _IF_H_ */
