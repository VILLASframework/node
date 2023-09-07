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
  using LimitHook::LimitHook;

  virtual void setRate(double rate, double maxRate = -1) {
    assert(maxRate > 0);

    int ratio = maxRate / rate;
    if (ratio == 0)
      ratio = 1;

    setRatio(ratio);
  }

  void setRatio(int r) { ratio = r; }

  virtual void start();

  virtual void parse(json_t *json);

  virtual Hook::Reason process(struct Sample *smp);
};

} // namespace node
} // namespace villas
