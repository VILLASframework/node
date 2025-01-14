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

int SignalList::parse(json_t *json) {
  int ret;

  if (!json_is_array(json))
    return -1;

  size_t i;
  json_t *json_signal;
  json_array_foreach(json, i, json_signal) {
    auto sig = std::make_shared<Signal>();
    if (!sig)
      throw MemoryAllocationError();

    ret = sig->parse(json_signal);
    if (ret)
      return ret;

    push_back(sig);
  }

  return 0;
}

SignalList::SignalList(unsigned len, enum SignalType typ) {
  char name[32];

  for (unsigned i = 0; i < len; i++) {
    snprintf(name, sizeof(name), "signal%u", i);

    auto sig = std::make_shared<Signal>(name, "", typ);
    if (!sig)
      throw RuntimeError("Failed to create signal list");

    push_back(sig);
  }
}

SignalList::SignalList(const char *dt) {
  int len, i = 0;
  char name[32], *e;
  enum SignalType typ;

  for (const char *t = dt; *t; t = e + 1) {
    len = strtoul(t, &e, 10);
    if (t == e)
      len = 1;

    typ = signalTypeFromFormatString(*e);
    if (typ == SignalType::INVALID)
      throw RuntimeError("Failed to create signal list");

    for (int j = 0; j < len; j++) {
      snprintf(name, sizeof(name), "signal%d", i++);

      auto sig = std::make_shared<Signal>(name, "", typ);
      if (!sig)
        throw RuntimeError("Failed to create signal list");

      push_back(sig);
    }
  }
}

void SignalList::dump(Logger logger, const union SignalData *data,
                      unsigned len) const {
  const char *pfx;
  bool abbrev = false;

  Signal::Ptr prevSig;
  unsigned i = 0;
  for (auto sig : *this) {
    // Check if this is a sequence of similar signals which can be abbreviated
    if (i >= 1 && i < size() - 1) {
      if (prevSig->isNext(*sig)) {
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
                 sig->toString(i < len ? &data[i] : nullptr));

  skip:
    prevSig = sig;
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

int SignalList::getIndexByName(const std::string &name) {
  unsigned i = 0;
  for (auto s : *this) {
    if (name == s->name)
      return i;

    i++;
  }

  return -1;
}

Signal::Ptr SignalList::getByName(const std::string &name) {
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
