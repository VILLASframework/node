/* Common entry point for all villas command line tools.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>

#include <villas/tool.hpp>

using namespace villas;

void Tool::staticHandler(int signal, siginfo_t *sinfo, void *ctx) {
  if (current_tool)
    current_tool->handler(signal, sinfo, ctx);
}

void Tool::printCopyright() {
  // cppcheck-suppress unknownMacro
  std::cout << PROJECT_NAME " " << CLR_BLU(PROJECT_VERSION)
            << " (built on " CLR_MAG(__DATE__) " " CLR_MAG(__TIME__) ")"
            << std::endl
            << " Copyright 2014-2025 The VILLASframework Authors" << std::endl
            << " Steffen Vogel <post@steffenvogel.de>" << std::endl;
}

// cppcheck-suppress unknownMacro
void Tool::printVersion() { std::cout << PROJECT_VERSION << std::endl; }

Tool::Tool(int ac, char *av[], const std::string &nme,
           const std::list<int> &sigs)
    : argc(ac), argv(av), name(nme), handlerSignals(sigs) {
  current_tool = this;

  logger = Log::get(name);
}

int Tool::run() {
  try {
    int ret;

    // Setup environment
    std::setlocale(LC_ALL, "en_US.UTF-8");

    logger->info("This is VILLASnode {} (built on {}, {})",
                 CLR_BLD(CLR_YEL(PROJECT_VERSION)), CLR_BLD(CLR_MAG(__DATE__)),
                 CLR_BLD(CLR_MAG(__TIME__)));

    ret = utils::signalsInit(staticHandler, handlerSignals);
    if (ret)
      throw RuntimeError("Failed to initialize signal subsystem");

    // Parse command line arguments
    parse();

    // Run tool
    ret = main();

    logger->info(CLR_GRN("Goodbye!"));

    return ret;
  } catch (const std::runtime_error &e) {
    logger->error("{}", e.what());

    return -1;
  }
}

Tool *Tool::current_tool = nullptr;
