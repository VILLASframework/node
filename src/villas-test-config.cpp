/* Main routine.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>
#include <unistd.h>

#include <villas/log.hpp>
#include <villas/node/config.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/tool.hpp>
#include <villas/utils.hpp>
#include <villas/version.hpp>

using namespace villas;
using namespace villas::node;

namespace villas {
namespace node {
namespace tools {

class TestConfig : public Tool {

public:
  TestConfig(int argc, char *argv[])
      : Tool(argc, argv, "test-config"), check(false), dump(false) {
    int ret;

    ret = memory::init(DEFAULT_NR_HUGEPAGES);
    if (ret)
      throw RuntimeError("Failed to initialize memory");
  }

protected:
  std::string uri;

  bool check;
  bool dump;

  void usage() {
    std::cout << "Usage: villas-test-config [OPTIONS] CONFIG" << std::endl
              << "  CONFIG is the path to an optional configuration file"
              << std::endl
              << "  OPTIONS is one or more of the following options:"
              << std::endl
              << "    -d LVL  set debug level" << std::endl
              << "    -V      show version and exit" << std::endl
              << "    -c      perform plausability checks on config"
              << std::endl
              << "    -D      dump config in JSON format" << std::endl
              << "    -h      show usage and exit" << std::endl
              << std::endl;

    printCopyright();
  }

  void parse() {
    int c;
    while ((c = getopt(argc, argv, "hcVD")) != -1) {
      switch (c) {
      case 'c':
        check = true;
        break;

      case 'D':
        dump = true;
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

    if (argc - optind < 1) {
      usage();
      exit(EXIT_FAILURE);
    }

    uri = argv[optind];
  }

  int main() {
    SuperNode sn;

    sn.parse(uri);

    // if (check)
    // 	sn.check();

    // if (dump)
    // 	json_dumpf(sn.getConfig(), stdout, JSON_INDENT(2));

    return 0;
  }
};

} // namespace tools
} // namespace node
} // namespace villas

int main(int argc, char *argv[]) {
  villas::node::tools::TestConfig t(argc, argv);

  return t.run();
}
