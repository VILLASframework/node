/** Timer/Counter unit test.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Steffen Vogel
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <chrono>
#include <criterion/criterion.h>
#include <villas/log.hpp>
#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/timer.hpp>

#include <villas/config.h>
#include "global.hpp"

Test(fpga, timer, .description = "Timer Counter")
{
	auto logger = loggerGetOrCreate("unittest:timer");

	size_t count = 0;

	for(auto& ip : state.cards.front()->ips) {
		// skip non-timer IPs
		if(*ip != villas::fpga::Vlnv("xilinx.com:ip:axi_timer:")) {
			continue;
		}

		logger->info("Testing {}", *ip);

		count++;

		auto timer = dynamic_cast<villas::fpga::ip::Timer&>(*ip);

		logger->info("Test simple waiting");
		timer.start(timer.getFrequency() / 10);
		cr_assert(timer.wait(), "Waiting failed");
		logger->info("-> passed");

		logger->info("Measure waiting time (1s)");
		timer.start(timer.getFrequency());

		const auto start = std::chrono::high_resolution_clock::now();
		timer.wait();
		const auto stop = std::chrono::high_resolution_clock::now();

		const int oneSecondInUs = 1000000;
		const auto duration = stop - start;
		const auto durationUs =
		        std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
		logger->info("-> time passed: {} us", durationUs);

		cr_assert(std::abs(durationUs - oneSecondInUs) < 0.01 * oneSecondInUs,
		          "Timer deviation > 1%%");

		logger->info(TXT_GREEN("Passed"));
	}

	cr_assert(count > 0, "No timer found");
}
