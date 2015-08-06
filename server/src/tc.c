/** Traffic control (tc): setup interface queuing desciplines.
 *
 * S2SS uses these functions to setup the network emulation feature.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include "utils.h"
#include "if.h"
#include "tc.h"

int tc_parse(config_setting_t *cfg, struct netem *em)
{
	em->valid = 0;

	if (config_setting_lookup_string(cfg, "distribution", &em->distribution))
		em->valid |= TC_NETEM_DISTR;
	if (config_setting_lookup_int(cfg, "delay", &em->delay))
		em->valid |= TC_NETEM_DELAY;
	if (config_setting_lookup_int(cfg, "jitter", &em->jitter))
		em->valid |= TC_NETEM_JITTER;
	if (config_setting_lookup_int(cfg, "loss", &em->loss))
		em->valid |= TC_NETEM_LOSS;
	if (config_setting_lookup_int(cfg, "duplicate", &em->duplicate))
		em->valid |= TC_NETEM_DUPL;
	if (config_setting_lookup_int(cfg, "corrupt", &em->corrupt))
		em->valid |= TC_NETEM_CORRUPT;

	/** @todo Validate netem config values */

	return 0;
}

int tc_reset(struct interface *i)
{
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "tc qdisc replace dev %s root pfifo_fast", i->name);

	debug(6, "Reset traffic control for interface '%s'", i->name);

	if (system2(cmd))
		error("Failed to add reset traffic control for interface '%s'", i->name);

	return 0;
}

int tc_prio(struct interface *i, tc_hdl_t handle, int bands)
{
	char cmd[128];
	int len = 0;
	int priomap[] = { 1, 2, 2, 2, 1, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 };

	len += snprintf(cmd+len, sizeof(cmd)-len,
		"tc qdisc replace dev %s root handle %u prio bands %u priomap",
		i->name, TC_HDL_MAJ(handle), bands + 3);

	for (int i = 0; i < 16; i++)
		len += snprintf(cmd+len, sizeof(cmd)-len, " %u", priomap[i] + bands);

	debug(6, "Replace master qdisc for interface '%s'", i->name);

	if (system2(cmd))
		error("Failed to add prio qdisc for interface '%s'", i->name);

	return 0;
}

int tc_netem(struct interface *i, tc_hdl_t parent, struct netem *em)
{
	int len = 0;
	char cmd[256];
	len += snprintf(cmd+len, sizeof(cmd)-len,
		"tc qdisc replace dev %s parent %u:%u netem",
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

	if (system2(cmd))
		error("Failed to add netem qdisc for interface '%s'", i->name);

	return 0;
}

int tc_mark(struct interface *i, tc_hdl_t flowid, int mark)
{
	char cmd[128];
	snprintf(cmd, sizeof(cmd),
		"tc filter replace dev %s protocol ip handle %u fw flowid %u:%u",
		i->name, mark, TC_HDL_MAJ(flowid), TC_HDL_MIN(flowid));

	debug(7, "Add traffic filter to interface '%s': fwmark %u => flowid %u:%u",
		i->name, mark, TC_HDL_MAJ(flowid), TC_HDL_MIN(flowid));

	if (system2(cmd))
		error("Failed to add fw_mark classifier for interface '%s'", i->name);

	return 0;
}
