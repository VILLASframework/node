/* Loadable / plugin support.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <iostream>
#include <list>
#include <string>

#include <fmt/ostream.h>
#include <jansson.h>

#include <villas/common.hpp>
#include <villas/config.hpp>
#include <villas/log.hpp>

namespace villas {
namespace plugin {

// Forward declarations
class Plugin;
class Registry;

extern Registry *registry;

template <typename T = Plugin> using List = std::list<T *>;

class SubRegistry {

public:
  virtual List<> lookup() = 0;
};

class Registry {

protected:
  List<> plugins;
  List<SubRegistry> registries;

public:
  Logger getLogger() { return Log::get("plugin:registry"); }

  void add(Plugin *p) { plugins.push_back(p); }

  void addSubRegistry(SubRegistry *sr) { registries.push_back(sr); }

  void remove(Plugin *p) { plugins.remove(p); }

  // Get all plugins including sub-registries
  List<> lookup() {
    List<> all;

    all.insert(all.end(), plugins.begin(), plugins.end());

    for (auto r : registries) {
      auto p = r->lookup();

      all.insert(all.end(), p.begin(), p.end());
    }

    return all;
  }

  // Get all plugins of specific type
  template <typename T = Plugin> List<T> lookup() {
    List<T> list;

    for (Plugin *p : lookup()) {
      T *t = dynamic_cast<T *>(p);
      if (t)
        list.push_back(t);
    }

    // Sort alphabetically
    list.sort(
        [](const T *a, const T *b) { return a->getName() < b->getName(); });

    return list;
  }

  // Get all plugins of specific type and name
  template <typename T = Plugin> T *lookup(const std::string &name) {
    for (T *p : lookup<T>()) {
      if (p->getName() != name)
        continue;

      return p;
    }

    return nullptr;
  }

  template <typename T = Plugin> void dump();
};

class Plugin {

  friend plugin::Registry;

protected:
  Logger logger;

public:
  Plugin();

  virtual ~Plugin();

  // Copying a plugin doesn't make sense, so explicitly deny it
  Plugin(Plugin const &) = delete;
  void operator=(Plugin const &) = delete;

  virtual void dump();

  // Get plugin name
  virtual std::string getName() const = 0;

  // Get plugin type
  virtual std::string getType() const = 0;

  // Get plugin description
  virtual std::string getDescription() const = 0;

  virtual Logger getLogger() {
    if (!logger) {
      auto name = fmt::format("{}:{}", getType(), getName());
      logger = Log::get(name);
    }

    return logger;
  }

  friend std::ostream &operator<<(std::ostream &os, const class Plugin &p) {
    return os << p.getName();
  }
};

template <typename T> void Registry::dump() {
  getLogger()->info("Available plugins:");

  for (Plugin *p : plugins) {
    T *t = dynamic_cast<T *>(p);
    if (t)
      getLogger()->info(" - {}: {}", p->getName(), p->getDescription());
  }
}

} // namespace plugin
} // namespace villas

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::plugin::Plugin> : public fmt::ostream_formatter {};
#endif
