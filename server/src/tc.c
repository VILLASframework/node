/** Traffic control (tc): setup interface queuing desciplines.
 *
 * S2SS uses these functions to setup the network emulation feature.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "utils.h"
#include "if.h"
#include "tc.h"

int tc_reset(struct interface *i)
{
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "tc qdisc del dev %s root", i->name);

	debug(6, "Reset traffic control for interface '%s'", i->name);
	debug(8, "System: %s", cmd);

	return system(cmd);
}

int tc_prio(struct interface *i, tc_hdl_t handle, int bands)
{
	char cmd[128];
	int len = 0;
	int priomap[] = { 1, 2, 2, 2, 1, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 };

	len += snprintf(cmd+len, sizeof(cmd)-len,
		"tc qdisc add dev %s root handle %u prio bands %u priomap",
		i->name, TC_HDL_MAJ(handle), bands + 3);

	for (int i = 0; i < 16; i++)
		len += snprintf(cmd+len, sizeof(cmd)-len, " %u", priomap[i] + bands);

	debug(6, "Replace master qdisc for interface '%s'", i->name);
	debug(8, "System: %s", cmd);

	return system(cmd);
}

int tc_netem(struct interface *i, tc_hdl_t parent, struct netem *em)
{
	int len = 0;
	char cmd[256];
	len += snprintf(cmd+len, sizeof(cmd)-len,
		"tc qdisc add dev %s parent %u:%u netem",
		i->name, TC_HDL_MAJ(parent), TC_HDL_MIN(parent));

	if (em->valid & TC_NETEM_DELAY) {
		len += snprintf(cmd+len, sizeof(cmd)-len, " delay %u", em->delay);

		if (em->valid & TC_NETEM_JITTER)
			len += snprintf(cmd+len, sizeof(cmd)-len, " %u", em->jitter);
		if (em->valid & TC_NETEM_DISTR)
			len += snprintf(cmd+len, sizeof(cmd)-len, " distribution %s", em->distribution);
	}

	if (em->valid & TC_NETEM_LOSS)
		len += snprintf(cmd+len, sizeof(cmd)-len, " loss random %u", em->loss);
	if (em->valid & TC_NETEM_DUPL)
		len += snprintf(cmd+len, sizeof(cmd)-len, " duplicate %u", em->duplicate);
	if (em->valid & TC_NETEM_CORRUPT)
		len += snprintf(cmd+len, sizeof(cmd)-len, " corrupt %u", em->corrupt);

	debug(6, "Setup netem qdisc for interface '%s'", i->name);
	debug(8, "System: %s", cmd);

	return system(cmd);
}

int tc_mark(struct interface *i, tc_hdl_t flowid, int mark)
{
	char cmd[128];
	snprintf(cmd, sizeof(cmd),
		"tc filter add dev %s protocol ip handle %u fw flowid %u:%u",
		i->name, mark, TC_HDL_MAJ(flowid), TC_HDL_MIN(flowid));

	debug(7, "Add traffic filter to interface '%s': fwmark %u => flowid %u:%u",
		i->name, mark, TC_HDL_MAJ(flowid), TC_HDL_MIN(flowid));
	debug(8, "System: %s", cmd);

	return system(cmd);

}
