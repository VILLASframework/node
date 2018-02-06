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
#include <netlink/route/qdisc/prio.h>

#include <linux/if_ether.h>

#include <villas/utils.h>

#include <villas/kernel/kernel.h>
#include <villas/kernel/if.h>
#include <villas/kernel/tc.h>
#include <villas/kernel/nl.h>

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
