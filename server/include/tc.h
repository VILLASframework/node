/** Setup interface queuing desciplines for network emulation
 *
 * We use the firewall mark to apply individual netem qdiscs
 * per node. Every node uses an own BSD socket.
 * By using so SO_MARK socket option (see socket(7))
 * we can classify traffic originating from a node seperately.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _TC_H_
#define _TC_H_

#include <stdint.h>

/** A type alias for TC handles.
 *
 * TC handles are used to construct a tree
 * of classes, qdiscs and filters.
 */
typedef uint32_t tc_hdl_t;

/** Concatenate 16 bit minor and majar parts to a 32 bit tc handle */
#define TC_HDL(maj, min)	((maj & 0xFFFF) << 16 | (min & 0xFFFF))
/** Get the major part of a tc handle */
#define TC_HDL_MAJ(h)		((h >> 16) & 0xFFFF)
/** Get the minor part of a tc handle */
#define TC_HDL_MIN(h)		((h >>  0) & 0xFFFF)
/** The root handle */
#define TC_HDL_ROOT		(0xFFFFFFFFU)

/* Bitfield for valid fields in struct netem */
#define TC_NETEM_DELAY		(1 << 0) /**< netem::delay is valid @see netem::valid */
#define TC_NETEM_JITTER		(1 << 1) /**< netem::jitter is valid @see netem::valid */
#define TC_NETEM_DISTR		(1 << 2) /**< netem::distribution is valid @see netem::valid */
#define TC_NETEM_LOSS		(1 << 3) /**< netem::loss is valid @see netem::valid */
#define TC_NETEM_CORRUPT	(1 << 4) /**< netem::corrupt is valid @see netem::valid */
#define TC_NETEM_DUPL		(1 << 5) /**< netem::duplicate is valid @see netem::valid */

struct interface;

/** Netem configuration settings
 *
 * This struct is used to pass the netem configuration
 * from config_parse_netem() to tc_netem()
 */
struct netem {
	/** Which fields of this struct contain valid data (TC_NETEM_*). */
	char valid;

	/** Delay distribution: uniform, normal, pareto, paretonormal */
	const char *distribution;
	/** Added delay (uS) */
	int delay;
	/** Delay jitter (uS) */
	int jitter;
	/** Random loss probability (%) */
	int loss;
	/** Packet corruption probability (%) */
	int corrupt;
	/** Packet duplication probability (%) */
	int duplicate;
};

/** Remove all queuing disciplines and filters 
 *
 * @param i The interface
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int tc_reset(struct interface *i);

/** Create a prio queueing discipline
 *
 * @param i The interface
 * @param handle The handle for the new qdisc
 * @param bands The number of classes for this new qdisc
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int tc_prio(struct interface *i, tc_hdl_t handle, int bands);

/** Add a new network emulator discipline
 *
 * @param i The interface
 * @param parent Make this qdisc a child of
 * @param em The netem settings
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int tc_netem(struct interface *i, tc_hdl_t parent, struct netem *em);

/** Add a new filter based on the netfilter mark
 *
 * @param i The interface
 * @param flowid The destination class for matched traffic
 * @param mark The netfilter firewall mark (sometime called 'fwmark')
 * @return
 *  - 0 on success
 *  - otherwise an error occured
*/
int tc_mark(struct interface *i, tc_hdl_t flowid, int mark);

#endif /* _TC_H_ */
