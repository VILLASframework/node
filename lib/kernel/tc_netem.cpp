/* Traffic control (tc): setup network emulation qdisc.
 *
 * VILLASnode uses these functions to setup the network emulation feature.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cmath>
#include <jansson.h>

#include <netlink/route/qdisc/netem.h>

#include <villas/exceptions.hpp>
#include <villas/kernel/if.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/kernel/nl-private.h>
#include <villas/kernel/nl.hpp>
#include <villas/kernel/tc_netem.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::utils;
using namespace villas::kernel;

static const double max_percent_value = 0xffffffff;

/*
 * Set the delay distribution. Latency/jitter must be set before applying.
 * @arg qdisc Netem qdisc.
 * @return 0 on success, error code on failure.
 */
static int rtnl_netem_set_delay_distribution_data(struct rtnl_qdisc *qdisc,
                                                  short *data, size_t len) {
  struct rtnl_netem *netem;

  if (!(netem = (struct rtnl_netem *)rtnl_tc_data(TC_CAST(qdisc))))
    return -1;

  if (len > MAXDIST)
    return -NLE_INVAL;

  netem->qnm_dist.dist_data = (int16_t *)calloc(len, sizeof(int16_t));

  size_t i;
  for (i = 0; i < len; i++)
    netem->qnm_dist.dist_data[i] = data[i];

  netem->qnm_dist.dist_size = len;
  netem->qnm_mask |= SCH_NETEM_ATTR_DIST;

  return 0;
}

// Customized version of rtnl_netem_set_delay_distribution() of libnl
static int set_delay_distribution(struct rtnl_qdisc *qdisc, json_t *json) {
  if (json_is_string(json))
    return rtnl_netem_set_delay_distribution(qdisc, json_string_value(json));
  else if (json_is_array(json)) {
    json_t *elm;
    size_t idx;
    size_t len = json_array_size(json);

    int16_t *data = new int16_t[len];
    if (!data)
      throw MemoryAllocationError();

    json_array_foreach(json, idx, elm) {
      if (!json_is_integer(elm))
        return -1;

      data[idx] = json_integer_value(elm);
    }

    return rtnl_netem_set_delay_distribution_data(qdisc, data, len);
  }

  return 0;
}

int villas::kernel::tc::netem_parse(struct rtnl_qdisc **netem, json_t *json) {
  int ret, val;

  json_t *json_limit = nullptr;
  json_t *json_delay = nullptr;
  json_t *json_delay_distribution = nullptr;
  json_t *json_delay_correlation = nullptr;
  json_t *json_jitter = nullptr;
  json_t *json_loss = nullptr;
  json_t *json_duplicate = nullptr;
  json_t *json_corruption = nullptr;

  json_error_t err;

  ret = json_unpack_ex(
      json, &err, 0,
      "{ s?: o, s?: o, s?: o, s?: o, s?: o, s?: o, s?: o, s?: o }",
      "distribution", &json_delay_distribution, "correlation",
      &json_delay_correlation, "limit", &json_limit, "delay", &json_delay,
      "jitter", &json_jitter, "loss", &json_loss, "duplicate", &json_duplicate,
      "corruption", &json_corruption);
  if (ret)
    throw ConfigError(json, err, "node-config-netem",
                      "Failed to parse setting network emulation settings");

  struct rtnl_qdisc *ne = rtnl_qdisc_alloc();
  if (!ne)
    throw MemoryAllocationError();

  rtnl_tc_set_kind(TC_CAST(ne), "netem");

  if (json_delay_distribution) {
    if (set_delay_distribution(ne, json_delay_distribution))
      throw ConfigError(json_delay_distribution,
                        "node-config-netem-distrobution",
                        "Invalid delay distribution in netem config");
  }

  if (json_delay_correlation) {
    double dval = json_number_value(json_delay_correlation);

    if (!json_is_number(json_delay_correlation) || dval < 0 || dval > 100)
      throw ConfigError(json_delay_correlation,
                        "Setting 'correlation' must be a positive integer "
                        "within the range [ 0, 100 ]");

    unsigned *pval = (unsigned *)&val;
    *pval = (unsigned)rint((dval / 100.) * max_percent_value);

    rtnl_netem_set_delay_correlation(ne, val);
  } else
    rtnl_netem_set_delay_correlation(ne, 0);

  if (json_limit) {
    val = json_integer_value(json_limit);

    if (!json_is_integer(json_limit) || val <= 0)
      throw ConfigError(json_limit,
                        "Setting 'limit' must be a positive integer");

    rtnl_netem_set_limit(ne, val);
  } else
    rtnl_netem_set_limit(ne, 0);

  if (json_delay) {
    val = json_integer_value(json_delay);

    if (!json_is_integer(json_delay) || val <= 0)
      throw ConfigError(json_delay,
                        "Setting 'delay' must be a positive integer");

    rtnl_netem_set_delay(ne, val);
  }

  if (json_jitter) {
    val = json_integer_value(json_jitter);

    if (!json_is_integer(json_jitter) || val <= 0)
      throw ConfigError(json_jitter,
                        "Setting 'jitter' must be a positive integer");

    rtnl_netem_set_jitter(ne, val);
  }

  if (json_loss) {
    double dval = json_number_value(json_loss);

    if (!json_is_number(json_loss) || dval < 0 || dval > 100)
      throw ConfigError(json_loss, "Setting 'loss' must be a positive integer "
                                   "within the range [ 0, 100 ]");

    unsigned *pval = (unsigned *)&val;
    *pval = (unsigned)rint((dval / 100.) * max_percent_value);

    rtnl_netem_set_loss(ne, val);
  }

  if (json_duplicate) {
    double dval = json_number_value(json_duplicate);

    if (!json_is_number(json_duplicate) || dval < 0 || dval > 100)
      throw ConfigError(json_duplicate,
                        "Setting 'duplicate' must be a positive integer within "
                        "the range [ 0, 100 ]");

    unsigned *pval = (unsigned *)&val;
    *pval = (unsigned)rint((dval / 100.) * max_percent_value);

    rtnl_netem_set_duplicate(ne, val);
  }

  if (json_corruption) {
    double dval = json_number_value(json_corruption);

    if (!json_is_number(json_corruption) || dval < 0 || dval > 100)
      throw ConfigError(json_corruption,
                        "Setting 'corruption' must be a positive integer "
                        "within the range [ 0, 100 ]");

    unsigned *pval = (unsigned *)&val;
    *pval = (unsigned)rint((dval / 100.) * max_percent_value);

    rtnl_netem_set_corruption_probability(ne, val);
  }

  *netem = ne;

  return 0;
}

char *villas::kernel::tc::netem_print(struct rtnl_qdisc *ne) {
  char *buf = nullptr;

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
    strcatf(&buf, "corruption %u%% ",
            rtnl_netem_get_corruption_probability(ne));

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

int villas::kernel::tc::netem(Interface *i, struct rtnl_qdisc **qd,
                              tc_hdl_t handle, tc_hdl_t parent) {
  int ret;
  struct nl_sock *sock = nl::init();
  struct rtnl_qdisc *q = *qd;

  ret = kernel::loadModule("sch_netem");
  if (ret)
    throw RuntimeError("Failed to load kernel module: sch_netem ({})", ret);

  rtnl_tc_set_link(TC_CAST(q), i->nl_link);
  rtnl_tc_set_parent(TC_CAST(q), parent);
  rtnl_tc_set_handle(TC_CAST(q), handle);
  //rtnl_tc_set_kind(TC_CAST(q), "netem");

  ret = rtnl_qdisc_add(sock, q, NLM_F_CREATE);

  *qd = q;

  auto logger = logging.get("kernel");
  logger->debug("Added netem qdisc to interface '{}'",
                rtnl_link_get_name(i->nl_link));

  return ret;
}
