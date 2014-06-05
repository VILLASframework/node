/** Interface related functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file if.h
 */

#ifndef _IF_H_
#define _IF_H_

#include <net/if.h>

/** Get the name of first interface which listens on addr.
 *
 * @param addr The address whose related interface is searched
 * @return
 *  - A pointer to the interface name (has to be freed by caller)
 *  - NULL if the address is not used on the system
 */
char* if_addrtoname(struct sockaddr *addr);

/** Change the SMP affinity of NIC interrupts.
 *
 * @param ifname The interface whose IRQs should be pinned
 * @param affinity A mask specifying which cores should handle this interrupt.
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int if_setaffinity(const char *ifname, int affinity);

#endif /* _IF_H_ */
