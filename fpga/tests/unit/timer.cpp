/* Timer/Counter unit test.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <criterion/criterion.h>
#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/timer.hpp>
#include <villas/log.hpp>

#include "global.hpp"
#include <villas/config.hpp>

// cppcheck-suppress unknownMacro
Test(fpga, timer, .description = "Timer Counter") {
  auto logger = villas::Log::get("unit-test:timer");

  size_t count = 0;
  for (auto &ip : state.cards.front()->ips) {
    // Skip non-timer IPs
    if (*ip != villas::fpga::Vlnv("xilinx.com:ip:axi_timer:"))
      continue;

    logger->info("Testing {}", *ip);

    auto timer = dynamic_cast<villas::fpga::ip::Timer &>(*ip);

    logger->info("Test simple waiting");
    timer.start(timer.getFrequency() / 10);
    cr_assert(timer.wait(), "Waiting failed");

    logger->info(CLR_GRN("Passed"));

    logger->info("Measure waiting time (1s)");

    timer.start(timer.getFrequency());
    const auto start = std::chrono::high_resolution_clock::now();

    timer.wait();
    const auto stop = std::chrono::high_resolution_clock::now();

    const int oneSecondInUs = 1000000;
    const auto duration = stop - start;
    const auto durationUs =
        std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

    cr_assert(std::abs(durationUs - oneSecondInUs) < 0.01 * oneSecondInUs,
              "Timer deviation > 1%%");

    logger->info(CLR_GRN("Passed:") " Time passed: {} us", durationUs);

    count++;
  }

  cr_assert(count > 0, "No timer found");
}
