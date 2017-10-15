/** Traffic control (tc): setup interface queuing desciplines.
 *
 * VILLASnode uses these functions to setup the network emulation feature.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <netlink/route/cls/fw.h>
#include <netlink/route/qdisc/netem.h>
#include <netlink/route/qdisc/prio.h>

#include <linux/if_ether.h>

#include "kernel/kernel.h"
#include "kernel/if.h"
#include "kernel/tc.h"
#include "kernel/nl.h"

#include "utils.h"

int tc_parse(struct rtnl_qdisc **netem, json_t *cfg)
{
	const char *str;
	int ret, val;

	json_t *cfg_distribution = NULL;
	json_t *cfg_limit = NULL;
	json_t *cfg_delay = NULL;
	json_t *cfg_jitter = NULL;
	json_t *cfg_loss = NULL;
	json_t *cfg_duplicate = NULL;
	json_t *cfg_corruption = NULL;

	json_error_t err;

		"distribution", &cfg_distribution,
		"limit", &cfg_limit,
		"delay", &cfg_delay,
		"jitter", &cfg_jitter,
		"loss", &cfg_loss,
		"duplicate", &cfg_duplicate,
		"corruption", &cfg_corruption
	ret = json_unpack_ex(cfg, &err, 0, "{ s?: o, s?: o, s?: o, s?: o, s?: o, s?: o, s?: o }",
	);
	if (ret)
		jerror(&err, "Failed to parse setting network emulation settings");

	struct rtnl_qdisc *ne = rtnl_qdisc_alloc();
	if (!ne)
		error("Failed to allocated memory!");

	rtnl_tc_set_kind(TC_CAST(ne), "netem");

	if (cfg_distribution) {
		str = json_string_value(cfg_distribution);
		if (!str)
			error("Setting 'distribution' must be a JSON string");

		if (rtnl_netem_set_delay_distribution(ne, str))
			error("Invalid delay distribution '%s' in netem config", str);
	}

	if (cfg_limit) {
		val = json_integer_value(cfg_limit);

		if (!json_is_integer(cfg_limit) || val <= 0)
			error("Setting 'limit' must be a positive integer");

		rtnl_netem_set_limit(ne, val);
	}
	else
		rtnl_netem_set_limit(ne, 0);

	if (cfg_delay) {
		val = json_integer_value(cfg_delay);

		if (!json_is_integer(cfg_delay) || val <= 0)
			error("Setting 'delay' must be a positive integer");

		rtnl_netem_set_delay(ne, val);
	}

	if (cfg_jitter) {
		val = json_integer_value(cfg_jitter);

		if (!json_is_integer(cfg_jitter) || val <= 0)
			error("Setting 'jitter' must be a positive integer");

		rtnl_netem_set_jitter(ne, val);
	}

	if (cfg_loss) {
		val = json_integer_value(cfg_loss);

		if (!json_is_integer(cfg_loss) || val < 0 || val > 100)
			error("Setting 'loss' must be a positive integer within the range [ 0, 100 ]");

		rtnl_netem_set_loss(ne, val);
	}

	if (cfg_duplicate) {
		val = json_integer_value(cfg_duplicate);

		if (!json_is_integer(cfg_duplicate) || val < 0 || val > 100)
			error("Setting 'duplicate' must be a positive integer within the range [ 0, 100 ]");

		rtnl_netem_set_duplicate(ne, val);
	}

	if (cfg_corruption) {
		val = json_integer_value(cfg_corruption);

		if (!json_is_integer(cfg_corruption) || val < 0 || val > 100)
			error("Setting 'corruption' must be a positive integer within the range [ 0, 100 ]");

		rtnl_netem_set_corruption_probability(ne, val);
	}

	*netem = ne;

	return 0;
}

char * tc_print(struct rtnl_qdisc *ne)
{
	char *buf = NULL;

	if (rtnl_netem_get_limit(ne) > 0)
		strcatf(&buf, "limit %upkts", rtnl_netem_get_limit(ne));

	if (rtnl_netem_get_delay(ne) > 0) {
		strcatf(&buf, "delay %.2fms ", rtnl_netem_get_delay(ne) / 1000.0);

		if (rtnl_netem_get_jitter(ne) > 0) {
			strcatf(&buf, "jitter %.2fms ", rtnl_netem_get_jitter(ne) / 1000.0);

			if (rtnl_netem_get_delay_correlation(ne) > 0)
				strcatf(&buf, "%u%% ", rtnl_netem_get_delay_correlation(ne));
		}
	}

	if (rtnl_netem_get_loss(ne) > 0) {
		strcatf(&buf, "loss %u%% ", rtnl_netem_get_loss(ne));

		if (rtnl_netem_get_loss_correlation(ne) > 0)
			strcatf(&buf, "%u%% ", rtnl_netem_get_loss_correlation(ne));
	}

	if (rtnl_netem_get_reorder_probability(ne) > 0) {
		strcatf(&buf, " reorder%u%% ", rtnl_netem_get_reorder_probability(ne));

		if (rtnl_netem_get_reorder_correlation(ne) > 0)
			strcatf(&buf, "%u%% ", rtnl_netem_get_reorder_correlation(ne));
	}

	if (rtnl_netem_get_corruption_probability(ne) > 0) {
		strcatf(&buf, "corruption %u%% ", rtnl_netem_get_corruption_probability(ne));

		if (rtnl_netem_get_corruption_correlation(ne) > 0)
			strcatf(&buf, "%u%% ", rtnl_netem_get_corruption_correlation(ne));
	}

	if (rtnl_netem_get_duplicate(ne) > 0) {
		strcatf(&buf, "duplication %u%% ", rtnl_netem_get_duplicate(ne));

		if (rtnl_netem_get_duplicate_correlation(ne) > 0)
			strcatf(&buf, "%u%% ", rtnl_netem_get_duplicate_correlation(ne));
	}

	return buf;
}

int tc_prio(struct interface *i, struct rtnl_qdisc **qd, tc_hdl_t handle, tc_hdl_t parent, int bands)
{
	int ret;
	struct nl_sock *sock = nl_init();
	struct rtnl_qdisc *q = rtnl_qdisc_alloc();

	ret = kernel_module_load("sch_prio");
	if (ret)
		error("Failed to load kernel module: sch_prio (%d)", ret);

	/* This is the default priomap used by the tc-prio qdisc
	 * We will use the first 'bands' bands internally */
	uint8_t map[] = QDISC_PRIO_DEFAULT_PRIOMAP;
	for (int i = 0; i < ARRAY_LEN(map); i++)
		map[i] += bands;

	rtnl_tc_set_link(TC_CAST(q), i->nl_link);
	rtnl_tc_set_parent(TC_CAST(q), parent);
	rtnl_tc_set_handle(TC_CAST(q), handle);
	rtnl_tc_set_kind(TC_CAST(q), "prio");

	rtnl_qdisc_prio_set_bands(q, bands + 3);
	rtnl_qdisc_prio_set_priomap(q, map, sizeof(map));

	ret = rtnl_qdisc_add(sock, q, NLM_F_CREATE | NLM_F_REPLACE);

	*qd = q;

	debug(LOG_TC | 3, "Added prio qdisc with %d bands to interface '%s'", bands, rtnl_link_get_name(i->nl_link));

	return ret;
}

int tc_netem(struct interface *i, struct rtnl_qdisc **qd, tc_hdl_t handle, tc_hdl_t parent)
{
	int ret;
	struct nl_sock *sock = nl_init();
	struct rtnl_qdisc *q = *qd;

	ret = kernel_module_load("sch_netem");
	if (ret)
		error("Failed to load kernel module: sch_netem (%d)", ret);

	rtnl_tc_set_link(TC_CAST(q), i->nl_link);
	rtnl_tc_set_parent(TC_CAST(q), parent);
	rtnl_tc_set_handle(TC_CAST(q), handle);
	//rtnl_tc_set_kind(TC_CAST(q), "netem");

	ret = rtnl_qdisc_add(sock, q, NLM_F_CREATE);

	*qd = q;

	debug(LOG_TC | 3, "Added netem qdisc to interface '%s'", rtnl_link_get_name(i->nl_link));

	return ret;
}

int tc_mark(struct interface *i, struct rtnl_cls **cls, tc_hdl_t flowid, uint32_t mark)
{
	int ret;
	struct nl_sock *sock = nl_init();
	struct rtnl_cls *c = rtnl_cls_alloc();

	ret = kernel_module_load("cls_fw");
	if (ret)
		error("Failed to load kernel module: cls_fw");

	rtnl_tc_set_link(TC_CAST(c), i->nl_link);
	rtnl_tc_set_handle(TC_CAST(c), mark);
	rtnl_tc_set_kind(TC_CAST(c), "fw");

	rtnl_cls_set_protocol(c, ETH_P_ALL);

	rtnl_fw_set_classid(c, flowid);
	rtnl_fw_set_mask(c, 0xFFFFFFFF);

	ret = rtnl_cls_add(sock, c, NLM_F_CREATE);

	*cls = c;

	debug(LOG_TC | 3, "Added fwmark classifier with mark %d to interface '%s'", mark, rtnl_link_get_name(i->nl_link));

	return ret;
}

int tc_reset(struct interface *i)
{
	struct nl_sock *sock = nl_init();

	/* We restore the default pfifo_fast qdisc, by deleting ours */
	return rtnl_qdisc_delete(sock, i->tc_qdisc);
}
