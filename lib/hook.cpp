/* Hook-releated functions.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cmath>
#include <cstring>

#include <villas/hook.hpp>
#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/path.hpp>
#include <villas/timing.hpp>
#include <villas/utils.hpp>

const char *hook_reasons[] = {"ok", "error", "skip-sample", "stop-processing"};

using namespace villas;
using namespace villas::node;

Hook::Hook(Path *p, Node *n, int fl, int prio, bool en)
    : logger(logging.get("hook")), factory(nullptr),
      state(fl & (int)Hook::Flags::BUILTIN
                ? State::CHECKED
                : State::INITIALIZED), // We dont need to parse builtin hooks
      flags(fl), priority(prio), enabled(en), path(p), node(n),
      config(nullptr) {}

void Hook::prepare(SignalList::Ptr sigs) {
  assert(state == State::CHECKED);

  signals = sigs->clone();

  prepare();

  state = State::PREPARED;
}

void Hook::parse(json_t *json) {
  int ret;
  json_error_t err;

  assert(state != State::STARTED);

  int prio = -1;
  int en = -1;

  ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: b }", "priority", &prio,
                       "enabled", &en);
  if (ret)
    throw ConfigError(json, err, "node-config-hook");

  if (prio >= 0)
    priority = prio;

  if (en >= 0)
    enabled = en;

  config = json;

  state = State::PARSED;
}

void SingleSignalHook::parse(json_t *json) {
  int ret;

  json_error_t err;
  json_t *json_signal;

  Hook::parse(json);

  ret = json_unpack_ex(json, &err, 0, "{ s: o }", "signal", &json_signal);
  if (ret)
    throw ConfigError(json, err, "node-config-hook");

  if (!json_is_string(json_signal))
    throw ConfigError(json_signal, "node-config-hook-signals",
                      "Invalid value for setting 'signal'");

  signalName = json_string_value(json_signal);
}

void SingleSignalHook::prepare() {
  Hook::prepare();

  // Setup mask
  int index = signals->getIndexByName(signalName.c_str());
  if (index < 0)
    throw RuntimeError("Failed to find signal {}", signalName);

  signalIndex = (unsigned)index;
}

// Multi Signal Hook

void MultiSignalHook::parse(json_t *json) {
  int ret;
  size_t i;

  json_error_t err;
  json_t *json_signals = nullptr;
  json_t *json_signal = nullptr;

  Hook::parse(json);

  ret = json_unpack_ex(json, &err, 0, "{ s?: o, s?: o }", "signals",
                       &json_signals, "signal", &json_signal);
  if (ret)
    throw ConfigError(json, err, "node-config-hook");

  if (json_signals) {
    if (!json_is_array(json_signals))
      throw ConfigError(json_signals, "node-config-hook-signals",
                        "Setting 'signals' must be a list of signal names");

    json_array_foreach(json_signals, i, json_signal) {
      if (!json_is_string(json_signal))
        throw ConfigError(json_signal, "node-config-hook-signals",
                          "Invalid value for setting 'signals'");

      const char *name = json_string_value(json_signal);

      signalNames.push_back(name);
    }
  } else if (json_signal) {
    if (!json_is_string(json_signal))
      throw ConfigError(json_signal, "node-config-hook-signals",
                        "Invalid value for setting 'signals'");

    const char *name = json_string_value(json_signal);

    signalNames.push_back(name);
  } else
    throw ConfigError(json, "node-config-hook-signals",
                      "Missing 'signals' setting");
}

void MultiSignalHook::prepare() {
  Hook::prepare();

  for (const auto &signalName : signalNames) {
    int index = signals->getIndexByName(signalName.c_str());
    if (index < 0)
      throw RuntimeError("Failed to find signal {}", signalName);

    signalIndices.push_back(index);
  }
}

void MultiSignalHook::check() {
  Hook::check();

  if (signalNames.size() == 0)
    throw RuntimeError("At least a single signal must be provided");
}
