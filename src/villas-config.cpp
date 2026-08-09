/* Inspect and convert VILLASnode configuration files.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdlib>
#include <exception>
#include <iostream>

#include <unistd.h>

#include <villas/json.hpp>
#include <villas/log.hpp>
#include <villas/node/json_schema.hpp>
#include <villas/super_node.hpp>
#include <villas/tool.hpp>

namespace villas {
namespace node {
namespace tools {

class Config : public Tool {

public:
  Config(int argc, char *argv[])
      : Tool(argc, argv, "config"), apply_migrations(false),
        apply_defaults(false), quiet(false) {}

protected:
  fs::path config_path;
  bool apply_migrations;
  bool apply_defaults;
  bool quiet;

  void usage() override {
    std::cout << "Usage: villas-config [OPTIONS] CONFIG" << std::endl
              << "  CONFIG is the path to a configuration file" << std::endl
              << "  OPTIONS is one or more of the following options:"
              << std::endl
              << "    -m      apply configuration migrations" << std::endl
              << "    -D      apply default values from the schemas"
              << std::endl
              << "    -q      do not write the configuration to stdout"
              << std::endl
              << "    -d LVL  set debug level" << std::endl
              << "    -V      show version and exit" << std::endl
              << "    -h      show usage and exit" << std::endl
              << std::endl;

    printCopyright();
  }

  void parse() override {
    int c;
    while ((c = getopt(argc, argv, "hVmDqd:")) != -1) {
      switch (c) {
      case 'm':
        apply_migrations = true;
        break;

      case 'D':
        apply_defaults = true;
        break;

      case 'q':
        quiet = true;
        break;

      case 'd':
        Log::getInstance().setLevel(optarg);
        break;

      case 'V':
        printVersion();
        exit(EXIT_SUCCESS);

      case 'h':
      case '?':
        usage();
        exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
      }
    }

    if (argc - optind != 1) {
      usage();
      exit(EXIT_FAILURE);
    }

    config_path = argv[optind];
  }

  int main() override {
    Json config;

    try {
      config = load_config_file(config_path, {
                                                 .allow_libconfig = true,
                                                 .allow_environment = true,
                                                 .allow_include = true,
                                                 .allow_comments = true,
                                             });

      SuperNode::validate(config, {
                                      .apply_migrations = apply_migrations,
                                      .apply_defaults = apply_defaults,
                                  });

      logger->info("Configuration validated successfully");
    } catch (const JsonError &error) {
      for (auto const &[ptr, msg] : error) {
        if (not ptr.empty())
          logger->error("config[{}]: {}", ptr, msg);
        else
          logger->error("config[/]: {}", msg);
      }

      return 1;
    }

    if (!quiet)
      std::cout << config.dump(2) << std::endl;

    return 0;
  }
};

} // namespace tools
} // namespace node
} // namespace villas

int main(int argc, char *argv[]) {
  villas::node::tools::Config t(argc, argv);

  return t.run();
}
