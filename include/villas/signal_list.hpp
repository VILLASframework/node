/* Signal metadata lits.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

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
  SignalList(json_t *json, std::function<Signal::Ptr(json_t *)> parse_signal =
                               Signal::fromJson);
  SignalList(std::string_view dt);
  SignalList(unsigned len, enum SignalType fmt);

  Ptr clone();

  void dump(villas::Logger logger, const union SignalData *data = nullptr,
            unsigned len = 0) const;

  json_t *toJson() const;

  int getIndexByName(std::string_view name);
  Signal::Ptr getByName(std::string_view name);
  Signal::Ptr getByIndex(unsigned idx);

  void
  parse(json_t *json_signals,
        std::function<Signal::Ptr(json_t *)> parse_signal = Signal::fromJson);
};

} // namespace node
} // namespace villas
