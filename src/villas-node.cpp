/* Main routine.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdlib>
#include <unistd.h>

#include <iomanip>
#include <iostream>

#include <villas/api.hpp>
#include <villas/api/request.hpp>
#include <villas/capabilities.hpp>
#include <villas/colors.hpp>
#include <villas/exceptions.hpp>
#include <villas/format.hpp>
#include <villas/hook.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/log.hpp>
#include <villas/memory.hpp>
#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/path.hpp>
#include <villas/plugin.hpp>
#include <villas/super_node.hpp>
#include <villas/tool.hpp>
#include <villas/utils.hpp>
#include <villas/version.hpp>
#include <villas/web.hpp>

#ifdef WITH_NODE_OPAL
#include <villas/nodes/opal.hpp>
#endif

using namespace villas;
using namespace villas::node;
using namespace villas::plugin;

namespace villas {
namespace node {
namespace tools {

class Node : public Tool {

public:
  Node(int argc, char *argv[]) : Tool(argc, argv, "node") {}

protected:
  SuperNode sn;

  std::string uri;
  bool showCapabilities = false;

  void handler(int signal, siginfo_t *sinfo, void *ctx) {
    switch (signal) {
    case SIGALRM:
      logger->info("Reached timeout. Terminating...");
      break;

    default:
      logger->info("Received {} signal. Terminating...", strsignal(signal));
    }

    sn.setState(State::STOPPING);
  }

  void usage() {
    std::cout
        << "Usage: villas-node [OPTIONS] [CONFIG]" << std::endl
        << "  OPTIONS is one or more of the following options:" << std::endl
        << "    -h      show this usage information" << std::endl
        << "    -d LVL  set logging level" << std::endl
        << "    -C      show capabilities in JSON format" << std::endl
        << "    -V      show the version of the tool" << std::endl
        << std::endl
        << "  CONFIG is the path to an optional configuration file" << std::endl
        << "         if omitted, VILLASnode will start without a configuration"
        << std::endl
        << "         and wait for provisioning over the web interface."
        << std::endl
        << std::endl
#ifdef WITH_NODE_OPAL
        << "Usage: villas-node OPAL_ASYNC_SHMEM_NAME OPAL_ASYNC_SHMEM_SIZE "
           "OPAL_PRINT_SHMEM_NAME"
        << std::endl
        << "  This type of invocation is used by OPAL-RT Asynchronous "
           "processes."
        << std::endl
        << "  See in the RT-LAB User Guide for more information." << std::endl
        << std::endl
#endif // WITH_NODE_OPAL

        << "Supported node-types:" << std::endl;
    for (auto p : registry->lookup<NodeFactory>()) {
      if (!p->isHidden())
        std::cout << " - " << std::left << std::setw(18) << p->getName()
                  << p->getDescription() << std::endl;
    }
    std::cout << std::endl;

    std::cout << "Supported IO formats:" << std::endl;
    for (auto p : registry->lookup<FormatFactory>()) {
      if (!p->isHidden())
        std::cout << " - " << std::left << std::setw(18) << p->getName()
                  << p->getDescription() << std::endl;
    }
    std::cout << std::endl;

#ifdef WITH_HOOKS
    std::cout << "Supported hooks:" << std::endl;
    for (auto p : registry->lookup<HookFactory>()) {
      if (!p->isHidden())
        std::cout << " - " << std::left << std::setw(18) << p->getName()
                  << p->getDescription() << std::endl;
    }
    std::cout << std::endl;
#endif // WITH_HOOKS

#ifdef WITH_API
    std::cout << "Supported API commands:" << std::endl;
    for (auto p : registry->lookup<api::RequestFactory>()) {
      if (!p->isHidden())
        std::cout << " - " << std::left << std::setw(18) << p->getName()
                  << p->getDescription() << std::endl;
    }
    std::cout << std::endl;
#endif // WITH_API

    printCopyright();
  }

  void parse() {
    // Check arguments
#ifdef WITH_NODE_OPAL
    if (argc != 4) {
      usage();
      exit(EXIT_FAILURE);
    }

    opal_register_region(argc, argv);

    uri = "villas-node.conf";
#else

    // Parse optional command line arguments
    int c;
    while ((c = getopt(argc, argv, "hCVd:")) != -1) {
      switch (c) {
      case 'V':
        printVersion();
        exit(EXIT_SUCCESS);

      case 'd':
        Log::getInstance().setLevel(optarg);
        break;

      case 'C':
        showCapabilities = true;
        break;

      case 'h':
      case '?':
        usage();
        exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
      }

      continue;
    }

    if (argc == optind + 1)
      uri = argv[optind];
    else if (argc != optind) {
      usage();
      exit(EXIT_FAILURE);
    }
#endif // ENABLE_OPAL_ASYNC
  }

  int main() { return showCapabilities ? capabilities() : daemon(); }

  int capabilities() {
    auto *json_caps = getCapabilities();

    json_dumpf(json_caps, stdout, JSON_INDENT(4));

    return 0;
  }

  int daemon() {
    if (!uri.empty())
      sn.parse(uri);
    else
      logger->warn("No configuration file specified. Starting unconfigured. "
                   "Use the API to configure this instance.");

    sn.check();
    sn.prepare();
    sn.start();
    sn.run();
    sn.stop();

    return 0;
  }
};

} // namespace tools
} // namespace node
} // namespace villas

int main(int argc, char *argv[]) {
  villas::node::tools::Node t(argc, argv);

  return t.run();
}
