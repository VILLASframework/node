/* Loadable / plugin support.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <iostream>
#include <new>
#include <string>
#include <type_traits>

#include <villas/plugin.hpp>

using namespace villas::plugin;

Registry *villas::plugin::registry = nullptr;

Plugin::Plugin() {
  if (registry == nullptr)
    registry = new Registry();

  registry->add(this);
}

Plugin::~Plugin() { registry->remove(this); }

void Plugin::dump() {
  getLogger()->info("Name: '{}' Description: '{}'", getName(),
                    getDescription());
}
