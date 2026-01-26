/* Configuration file parsing.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdio>

#include <jansson.h>
#include <unistd.h>

#include <villas/fs.hpp>
#include <villas/log.hpp>
#include <villas/node/config.hpp>

#ifdef WITH_CONFIG
#include <libconfig.h>
#endif

namespace villas {
namespace node {

class Config {
private:
  Logger logger;
  fs::path configPath;

public:
  json_t *root;

  Config();
  Config(fs::path path);

  Config(Config const &) = delete;
  Config &operator=(Config const &) = delete;
  Config(Config &&) = delete;
  Config &operator=(Config &&) = delete;
  ~Config();

  json_t *load(fs::path path, bool resolveIncludes = true,
               bool resolveEnvVars = true);

  fs::path const &getConfigPath() const { return configPath; }
};

} // namespace node
} // namespace villas
