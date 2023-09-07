/* Netlink Types (Private).
 *
 * Copied from: https://github.com/tgraf/libnl/blob/master/include/netlink-private/types.h
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-FileCopyrightText: 2003-2013 Thomas Graf <tgraf@suug.ch>
 * SPDX-FileCopyrightText: 2013 Sassano Systems LLC <joe@sassanosystems.com>
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SCH_NETEM_ATTR_DIST 0x2000

struct rtnl_netem_corr {
  uint32_t nmc_delay;
  uint32_t nmc_loss;
  uint32_t nmc_duplicate;
};

struct rtnl_netem_reo {
  uint32_t nmro_probability;
  uint32_t nmro_correlation;
};

struct rtnl_netem_crpt {
  uint32_t nmcr_probability;
  uint32_t nmcr_correlation;
};

struct rtnl_netem_dist {
  int16_t *dist_data;
  size_t dist_size;
};

struct rtnl_netem {
  uint32_t qnm_latency;
  uint32_t qnm_limit;
  uint32_t qnm_loss;
  uint32_t qnm_gap;
  uint32_t qnm_duplicate;
  uint32_t qnm_jitter;
  uint32_t qnm_mask;
  struct rtnl_netem_corr qnm_corr;
  struct rtnl_netem_reo qnm_ro;
  struct rtnl_netem_crpt qnm_crpt;
  struct rtnl_netem_dist qnm_dist;
};

void *rtnl_tc_data(struct rtnl_tc *tc);

#ifdef __cplusplus
}
#endif
