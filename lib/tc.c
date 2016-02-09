/** Traffic control (tc): setup interface queuing desciplines.
 *
 * S2SS uses these functions to setup the network emulation feature.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <netlink/route/cls/fw.h>
#include <netlink/route/qdisc/netem.h>
#include <netlink/route/qdisc/prio.h>

#include <linux/if_ether.h>

#include "utils.h"
#include "if.h"
#include "tc.h"
#include "nl.h"

int tc_parse(config_setting_t *cfg, struct rtnl_qdisc **netem)
{
	const char *str;
	int val;
	
	struct rtnl_qdisc *ne = rtnl_qdisc_alloc();
	if (!ne)
		error("Failed to allocated memory!");

	rtnl_tc_set_kind(TC_CAST(ne), "netem");

	if (config_setting_lookup_string(cfg, "distribution", &str)) {
		if (rtnl_netem_set_delay_distribution(ne, str))
			cerror(cfg, "Invalid delay distribution '%s' in netem config", str);
	}
	
	if (config_setting_lookup_int(cfg, "limit", &val)) {
		if (val <= 0)
			cerror(cfg, "Invalid value '%d' for limit setting", val);

		rtnl_netem_set_limit(ne, val);
	}
	else
		rtnl_netem_set_limit(ne, 0);

	if (config_setting_lookup_int(cfg, "delay", &val)) {
		if (val <= 0)
			cerror(cfg, "Invalid value '%d' for delay setting", val);

		rtnl_netem_set_delay(ne, val);
	}

	if (config_setting_lookup_int(cfg, "jitter", &val)) {
		if (val <= 0)
			cerror(cfg, "Invalid value '%d' for jitter setting", val);

		rtnl_netem_set_jitter(ne, val);
	}

	if (config_setting_lookup_int(cfg, "loss", &val)) {
		if (val < 0 || val > 100)
			cerror(cfg, "Invalid percentage value '%d' for loss setting", val);
		
		rtnl_netem_set_loss(ne, val);
	}

	if (config_setting_lookup_int(cfg, "duplicate", &val)) {
		if (val < 0 || val > 100)
			cerror(cfg, "Invalid percentage value '%d' for duplicate setting", val);

		rtnl_netem_set_duplicate(ne, val);
	}

	if (config_setting_lookup_int(cfg, "corruption", &val)) {
		if (val < 0 || val > 100)
			cerror(cfg, "Invalid percentage value '%d' for corruption setting", val);
		
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
	struct nl_sock *sock = nl_init();
	struct rtnl_qdisc *q = rtnl_qdisc_alloc();

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

	int ret = rtnl_qdisc_add(sock, q, NLM_F_CREATE | NLM_F_REPLACE);

	*qd = q;
	
	return ret;
}

int tc_netem(struct interface *i, struct rtnl_qdisc **qd, tc_hdl_t handle, tc_hdl_t parent)
{
	struct nl_sock *sock = nl_init();
	struct rtnl_qdisc *q = *qd;

	rtnl_tc_set_link(TC_CAST(q), i->nl_link);
	rtnl_tc_set_parent(TC_CAST(q), parent);
	rtnl_tc_set_handle(TC_CAST(q), handle);
	//rtnl_tc_set_kind(TC_CAST(q), "netem");

	int ret = rtnl_qdisc_add(sock, q, NLM_F_CREATE);

	*qd = q;
	
	return ret;
}

int tc_mark(struct interface *i, struct rtnl_cls **cls, tc_hdl_t flowid, uint32_t mark)
{
	struct nl_sock *sock = nl_init();
	struct rtnl_cls *c = rtnl_cls_alloc();

	rtnl_tc_set_link(TC_CAST(c), i->nl_link);
	rtnl_tc_set_handle(TC_CAST(c), mark);
	rtnl_tc_set_kind(TC_CAST(c), "fw");

	rtnl_cls_set_protocol(c, ETH_P_ALL);
	
	rtnl_fw_set_classid(c, flowid);
	rtnl_fw_set_mask(c, 0xFFFFFFFF);

	int ret = rtnl_cls_add(sock, c, NLM_F_CREATE);

	*cls = c;
	
	return ret;
}

int tc_reset(struct interface *i)
{
	struct nl_sock *sock = nl_init();

	/* We restore the default pfifo_fast qdisc, by deleting ours */
	return rtnl_qdisc_delete(sock, i->tc_qdisc); 
}
