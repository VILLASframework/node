/* Configuration file parsing.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdlib>
#include <filesystem>
#include <string>

#include <fnmatch.h>
#include <glob.h>
#include <libgen.h>
#include <linux/limits.h>
#include <unistd.h>

#include <villas/boxes.hpp>
#include <villas/config_class.hpp>
#include <villas/config_helper.hpp>
#include <villas/json.hpp>
#include <villas/log.hpp>
#include <villas/node/config.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/utils.hpp>

#ifdef WITH_CONFIG
#include <libconfig.h>
#endif

using namespace villas;
using namespace villas::node;
using namespace std::string_view_literals;

Config::Config() : logger(Log::get("config")), root(nullptr) {}

Config::Config(fs::path path) : Config() { root = load(std::move(path)); }

Config::~Config() { json_decref(root); }

json_t *Config::load(fs::path path, bool resolveInc, bool resolveEnvVars) {
  if (path == "-") {
    logger->info("Reading configuration from standard input");
    configPath = fs::current_path();
    auto opts = config_json::options_t{
        .expand_substitutions = resolveEnvVars,
        .expand_includes = resolveInc,
        .expand_deprecated = true,
        .include_directories = std::span{&configPath, 1},
    };
    return config_json::load_config(stdin, opts).to_libjansson().release();
  } else {
    logger->info("Reading configuration from file: {}", path.string());
    configPath = fs::canonical(path).parent_path();
    auto opts = config_json::options_t{
        .expand_substitutions = resolveEnvVars,
        .expand_includes = resolveInc,
        .expand_deprecated = true,
        .include_directories = std::span{&configPath, 1},
    };
    return config_json::load_config_file(path, opts).to_libjansson().release();
  }
}
