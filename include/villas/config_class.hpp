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

#include <functional>
#include <regex>

#include <villas/log.hpp>
#include <villas/node/config.hpp>

#ifdef WITH_CONFIG
#include <libconfig.h>
#endif

namespace villas {
namespace node {

class Config {

protected:
  using str_walk_fcn_t = std::function<json_t *(json_t *)>;

  Logger logger;

  std::list<std::string> includeDirectories;

  // Check if file exists on local system.
  static bool isLocalFile(const std::string &uri) {
    return access(uri.c_str(), F_OK) != -1;
  }

  // Decode configuration file.
  json_t *decode(FILE *f);

#ifdef WITH_CONFIG
  // Convert libconfig .conf file to libjansson .json file.
  json_t *libconfigDecode(FILE *f);

  static const char **includeFuncStub(config_t *cfg, const char *include_dir,
                                      const char *path, const char **error);

  const char **includeFunc(config_t *cfg, const char *include_dir,
                           const char *path, const char **error);
#endif // WITH_CONFIG

  // Load configuration from standard input (stdim).
  FILE *loadFromStdio();

  // Load configuration from local file.
  FILE *loadFromLocalFile(const std::string &u);

  std::list<std::string> resolveIncludes(const std::string &name);

  void resolveEnvVars(std::string &text);

  // Resolve custom include directives.
  json_t *expandIncludes(json_t *in);

  // To shell-like subsitution of environment variables in strings.
  json_t *expandEnvVars(json_t *in);

  // Run a callback function for each string in the config
  json_t *walkStrings(json_t *in, str_walk_fcn_t cb);

  // Get the include dirs
  std::list<std::string> getIncludeDirectories(FILE *f) const;

public:
  json_t *root;

  Config();
  Config(const std::string &u);

  ~Config();

  json_t *load(std::FILE *f, bool resolveIncludes = true,
               bool resolveEnvVars = true);

  json_t *load(const std::string &u, bool resolveIncludes = true,
               bool resolveEnvVars = true);
};

} // namespace node
} // namespace villas
