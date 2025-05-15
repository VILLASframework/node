/* Decimate hook.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/hook.hpp>

namespace villas {
namespace node {

class DecimateHook : public LimitHook {

protected:
  int ratio;
  bool renumber;
  unsigned counter;

public:
  DecimateHook(Path *p, Node *n, int fl, int prio, bool en = true)
      : LimitHook(p, n, fl, prio, en), ratio(1), renumber(false), counter(0) {}

  void setRate(double rate, double maxRate = -1) override {
    assert(maxRate > 0);

    int ratio = maxRate / rate;
    if (ratio == 0)
      ratio = 1;

    setRatio(ratio);
  }

  void setRatio(int r) { ratio = r; }

  void start() override;

  void parse(json_t *json) override;

  Hook::Reason process(struct Sample *smp) override;
};

} // namespace node
} // namespace villas
