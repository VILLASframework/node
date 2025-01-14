
/* Universal Data-exchange API request.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <jansson.h>

#include <villas/signal.hpp>

namespace villas {
namespace node {
namespace api {
namespace universal {

enum PayloadType { SAMPLES = 0, EVENTS = 1 };

// Channel (uAPI) is a synonym for signal (VILLAS)
class Channel {
public:
  std::string description;
  PayloadType payload;
  double range_min;
  double range_max;
  std::vector<std::string> range_options;
  double rate;
  bool readable;
  bool writable;

  using Ptr = std::shared_ptr<Channel>;

  void parse(json_t *json);
  json_t *toJson(Signal::Ptr sig) const;
};

class ChannelList : public std::vector<Channel::Ptr> {

public:
  void parse(json_t *json, bool readable, bool writable);
};

} // namespace universal
} // namespace api
} // namespace node
} // namespace villas
