/**
 * Setup interface queuing desciplines for network emulation
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _TC_H_
#define _TC_H_

#include <stdint.h>

/* Some helper for TC handles */
typedef uint32_t tc_hdl_t;

#define TC_HDL(maj, min)	((maj & 0xFFFF) << 16 | (min & 0xFFFF))
#define TC_HDL_MAJ(h)		((h >> 16) & 0xFFFF)
#define TC_HDL_MIN(h)		((h >>  0) & 0xFFFF)
#define TC_HDL_ROOT		(0xFFFFFFFFU)

/* Bitfield for valid fields in struct netem */
#define TC_NETEM_LIMIT		(1 << 0)
#define TC_NETEM_DELAY		(1 << 1)
#define TC_NETEM_JITTER		(1 << 2)
#define TC_NETEM_DISTR		(1 << 3)
#define TC_NETEM_LOSS		(1 << 4)
#define TC_NETEM_CORRUPT	(1 << 5)
#define TC_NETEM_DUPL		(1 << 6)

struct interface;

struct netem {
	/** Which fields of this struct contain valid data (TC_NETEM_*). */
	char valid;

	/** Delay distribution: uniform, normal, pareto, paretonormal */
	const char *distribution;
	/** Fifo limit (packets) */
	int limit;
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
 */
int tc_reset(struct interface *i);

/** Create a prio queueing discipline
 *
 * @param i The interface
 * @param parent
 * @param handle
 * @param bands
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int tc_prio(struct interface *i, tc_hdl_t handle, int bands);

/** Add a new network emulator discipline
 *
 * @param i The interface
 * @param parent Make this
 */
int tc_netem(struct interface *i, tc_hdl_t parent, struct netem *em);

/** Add a new fwmark filter */
int tc_mark(struct interface *i, tc_hdl_t flowid, int mark);

#endif /* _TC_H_ */
