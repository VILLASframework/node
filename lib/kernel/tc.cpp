/* Traffic control (tc): setup interface queuing desciplines.
 *
 * VILLASnode uses these functions to setup the network emulation feature.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <netlink/route/cls/fw.h>
#include <netlink/route/qdisc/prio.h>

#include <linux/if_ether.h>

#include <villas/exceptions.hpp>
#include <villas/kernel/if.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/kernel/nl.hpp>
#include <villas/kernel/tc.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::kernel;

int villas::kernel::tc::prio(Interface *i, struct rtnl_qdisc **qd,
                             tc_hdl_t handle, tc_hdl_t parent, int bands) {
  int ret;
  struct nl_sock *sock = nl::init();
  struct rtnl_qdisc *q = rtnl_qdisc_alloc();

  ret = kernel::loadModule("sch_prio");
  if (ret)
    throw RuntimeError("Failed to load kernel module: sch_prio ({})", ret);

  /* This is the default priomap used by the tc-prio qdisc
   * We will use the first 'bands' bands internally */
  uint8_t map[] = QDISC_PRIO_DEFAULT_PRIOMAP;
  for (unsigned i = 0; i < ARRAY_LEN(map); i++)
    map[i] += bands;

  rtnl_tc_set_link(TC_CAST(q), i->nl_link);
  rtnl_tc_set_parent(TC_CAST(q), parent);
  rtnl_tc_set_handle(TC_CAST(q), handle);
  rtnl_tc_set_kind(TC_CAST(q), "prio");

  rtnl_qdisc_prio_set_bands(q, bands + 3);
  rtnl_qdisc_prio_set_priomap(q, map, sizeof(map));

  ret = rtnl_qdisc_add(sock, q, NLM_F_CREATE | NLM_F_REPLACE);

  *qd = q;

  auto logger = logging.get("kernel");
  logger->debug("Added prio qdisc with {} bands to interface '{}'", bands,
                rtnl_link_get_name(i->nl_link));

  return ret;
}

int villas::kernel::tc::mark(Interface *i, struct rtnl_cls **cls,
                             tc_hdl_t flowid, uint32_t mark) {
  int ret;
  struct nl_sock *sock = nl::init();
  struct rtnl_cls *c = rtnl_cls_alloc();

  ret = kernel::loadModule("cls_fw");
  if (ret)
    throw RuntimeError("Failed to load kernel module: cls_fw");

  rtnl_tc_set_link(TC_CAST(c), i->nl_link);
  rtnl_tc_set_handle(TC_CAST(c), mark);
  rtnl_tc_set_kind(TC_CAST(c), "fw");

  rtnl_cls_set_protocol(c, ETH_P_ALL);

  rtnl_fw_set_classid(c, flowid);
  rtnl_fw_set_mask(c, 0xFFFFFFFF);

  ret = rtnl_cls_add(sock, c, NLM_F_CREATE);

  *cls = c;

  auto logger = logging.get("kernel");
  logger->debug("Added fwmark classifier with mark {} to interface '{}'", mark,
                rtnl_link_get_name(i->nl_link));

  return ret;
}

int villas::kernel::tc::reset(Interface *i) {
  struct nl_sock *sock = nl::init();

  // We restore the default pfifo_fast qdisc, by deleting ours
  return rtnl_qdisc_delete(sock, i->tc_qdisc);
}
