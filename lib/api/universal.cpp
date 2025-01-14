/* API Response.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cmath>
#include <limits>

#include <villas/api/universal.hpp>
#include <villas/exceptions.hpp>

using namespace villas::node::api::universal;

void ChannelList::parse(json_t *json, bool readable, bool writable) {
  if (!json_is_array(json))
    throw ConfigError(json, "node-config-node-api-signals",
                      "Signal list of API node must be an array");

  clear();

  size_t i;
  json_t *json_channel;
  json_array_foreach(json, i, json_channel) {
    auto channel = std::make_shared<Channel>();

    channel->parse(json_channel);
    channel->readable = readable;
    channel->writable = writable;

    push_back(channel);
  }
}

void Channel::parse(json_t *json) {
  const char *desc = nullptr;
  const char *pl = nullptr;
  json_t *json_range = nullptr;

  json_error_t err;
  int ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: o, s?: F }",
                           "description", &desc, "payload", &pl, "range",
                           &json_range, "rate", &rate);
  if (ret)
    throw ConfigError(json, err, "node-config-node-api-signals");

  if (desc)
    description = desc;

  if (pl) {
    if (!strcmp(pl, "samples"))
      payload = PayloadType::SAMPLES;
    else if (!strcmp(pl, "events"))
      payload = PayloadType::EVENTS;
    else
      throw ConfigError(json, "node-config-node-api-signals-payload",
                        "Invalid payload type: {}", pl);
  }

  range_min = std::numeric_limits<double>::quiet_NaN();
  range_max = std::numeric_limits<double>::quiet_NaN();
  if (json_range) {
    if (json_is_array(json_range)) {
      ret = json_unpack_ex(json, &err, 0, "{ s?: F, s?: F }", "min", &range_min,
                           "max", &range_max);
      if (ret)
        throw ConfigError(json, err, "node-config-node-api-signals-range",
                          "Failed to parse channel range");
    } else if (json_is_object(json_range)) {
      size_t i;
      json_t *json_option;

      range_options.clear();

      json_array_foreach(json_range, i, json_option) {
        if (!json_is_string(json_option))
          throw ConfigError(json, err, "node-config-node-api-signals-range",
                            "Channel range options must be strings");

        auto *option = json_string_value(json_option);
        range_options.push_back(option);
      }
    } else
      throw ConfigError(json, "node-config-node-api-signals-range",
                        "Channel range must be an array or object");
  }
}

json_t *Channel::toJson(Signal::Ptr sig) const {
  json_error_t err;
  json_t *json_ch = json_pack_ex(
      &err, 0, "{ s: s, s: s, s: b, s: b }", "id", sig->name.c_str(),
      "datatype", signalTypeToString(sig->type).c_str(), "readable",
      (int)readable, "writable", (int)writable);

  if (!description.empty())
    json_object_set(json_ch, "description", json_string(description.c_str()));

  if (!sig->unit.empty())
    json_object_set(json_ch, "unit", json_string(sig->unit.c_str()));

  if (rate > 0)
    json_object_set(json_ch, "rate", json_real(rate));

  switch (payload) {
  case PayloadType::EVENTS:
    json_object_set(json_ch, "payload", json_string("events"));
    break;

  case PayloadType::SAMPLES:
    json_object_set(json_ch, "payload", json_string("samples"));
    break;

  default: {
  }
  }

  switch (sig->type) {
  case SignalType::FLOAT: {
    if (std::isnan(range_min) && std::isnan(range_max))
      break;

    json_t *json_range = json_object();

    if (!std::isnan(range_min))
      json_object_set(json_range, "min", json_real(range_min));

    if (!std::isnan(range_max))
      json_object_set(json_range, "max", json_real(range_max));

    json_object_set(json_ch, "range", json_range);
    break;
  }

  default: {
  }
  }

  return json_ch;
}
