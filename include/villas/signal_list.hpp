/* Signal metadata lits.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <list>
#include <memory>

#include <jansson.h>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/signal.hpp>

namespace villas {
namespace node {

class SignalList : public std::vector<Signal::Ptr> {

public:
  using Ptr = std::shared_ptr<SignalList>;

  SignalList() {}

  SignalList(unsigned len, enum SignalType fmt);
  SignalList(const char *dt);
  SignalList(json_t *json) {
    int ret = parse(json);
    if (ret)
      throw RuntimeError("Failed to parse signal list");
  }

  int parse(json_t *json);

  Ptr clone();

  void dump(villas::Logger logger, const union SignalData *data = nullptr,
            unsigned len = 0) const;

  json_t *toJson() const;

  int getIndexByName(const std::string &name);
  Signal::Ptr getByName(const std::string &name);
  Signal::Ptr getByIndex(unsigned idx);
};

} // namespace node
} // namespace villas
