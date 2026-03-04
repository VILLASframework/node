/* Signal metadata list.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/exceptions.hpp>
#include <villas/list.hpp>
#include <villas/signal.hpp>
#include <villas/signal_data.hpp>
#include <villas/signal_list.hpp>
#include <villas/signal_type.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

SignalList::SignalList(json_t *json_signals,
                       std::function<Signal::Ptr(json_t *json, unsigned index)> parse_signal) {
  parse(json_signals, parse_signal);
}

SignalList::SignalList(unsigned length, enum SignalType typ) {
  auto typ_str = signalTypeToString(typ);

  auto *json_signals = json_pack("{ s: s, s: s, s: i }", "name", "signal",
                                 "type", typ_str.c_str(), "length", length);

  parse(json_signals);
}

SignalList::SignalList(std::string_view dt) {
  json_t *json_signals = json_array();

  int i = 0;
  char *e;

  auto *dtc = dt.data();

  for (const char *t = dtc; *t; t = e + 1) {
    auto length = strtoul(t, &e, 10);
    if (t == e)
      length = 1;

    auto name = fmt::format("signal_{}", i++);

    auto typ = signalTypeFromFormatString(*e);
    if (typ == SignalType::INVALID)
      throw RuntimeError("Failed to create signal list");

    auto typ_str = signalTypeToString(typ);

    auto *json_signal = json_pack("{ s: s, s: s, s: i }", "name", name.c_str(),
                                  "type", typ_str.c_str(), "length", length);

    json_array_append_new(json_signals, json_signal);
  }

  parse(json_signals);
}

void SignalList::parse(json_t *json_signals,
                       std::function<Signal::Ptr(json_t *json, unsigned index)> parse_signal) {
  clear();

  if (json_is_string(json_signals)) {
    SignalList(json_string_value(json_signals));
  } else if (json_is_object(json_signals)) {
    auto *json_tmp = json_signals;

    json_signals = json_array();
    json_array_append_new(json_signals, json_tmp);
  } else if (!json_is_array(json_signals)) {
    throw ConfigError(json_signals, "node-config-node-signals",
                      "Invalid signal list");
  }

  unsigned index = 0;
  
  size_t i;
  json_t *json_signal;
  json_array_foreach(json_signals, i, json_signal) {
    if (!json_is_object(json_signal)) {
      throw ConfigError(json_signal,
                        "node-config-node-signal"
                        "Signal definitions must be a JSON object");
    }

    std::string baseName = "signal";
    bool appendIndex = false;

    int length = 1;
    const char *nme = nullptr;

    int ret = json_unpack(json_signal, "{ s?: i, s?: s }", "length", &length,
                          "name", &nme);
    if (ret) {
      throw ConfigError(json_signal, "node-config-node-signal",
                        "Failed to parse signal definition");
    }

    if (length > 1) {
      appendIndex = true;
    }

    if (nme) {
      baseName = nme;
    }

    Signal::Ptr previousSignal;
    for (int j = 0; j < length; j++) {
      if (appendIndex) {
        auto name = fmt::format("{}_{}", baseName, j);
        json_object_set_new(json_signal, "name", json_string(name.c_str()));
      }

      auto signal = parse_signal(json_signal, index++);
      if (!signal)
        throw ConfigError(json_signal, "node-config-node-signal",
                          "Failed to parse signal definition");

      push_back(signal);
      
      signal->previous = previousSignal;
      previousSignal = signal;
    }
  }
}

void SignalList::dump(Logger logger, const union SignalData *data,
                      unsigned len) const {
  const char *pfx;
  bool abbrev = false;

  Signal::Ptr previousSignal;
  unsigned i = 0;
  for (auto signal : *this) {
    // Check if this is a sequence of similar signals which can be abbreviated
    if (i >= 1 && i < size() - 1) {
      if (signal->previous == previousSignal) {
        abbrev = true;
        goto skip;
      }
    }

    if (abbrev) {
      pfx = "...";
      abbrev = false;
    } else
      pfx = "   ";

    logger->info(" {}{:>3}: {}", pfx, i,
                 signal->toString(i < len ? &data[i] : nullptr));

  skip:
    previousSignal = signal;
    i++;
  }
}

json_t *SignalList::toJson() const {
  json_t *json_signals = json_array();

  for (const auto &sig : *this)
    json_array_append_new(json_signals, sig->toJson());

  return json_signals;
}

Signal::Ptr SignalList::getByIndex(unsigned idx) { return this->at(idx); }

int SignalList::getIndexByName(std::string_view name) {
  unsigned i = 0;
  for (auto s : *this) {
    if (name == s->name)
      return i;

    i++;
  }

  return -1;
}

Signal::Ptr SignalList::getByName(std::string_view name) {
  for (auto s : *this) {
    if (name == s->name)
      return s;
  }

  return Signal::Ptr();
}

SignalList::Ptr SignalList::clone() {
  auto l = std::make_shared<SignalList>();

  for (auto s : *this)
    l->push_back(s);

  return l;
}
