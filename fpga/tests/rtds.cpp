/** RTDS AXI-Stream RTT unit test.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2018, Steffen Vogel, Daniel Krebs
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

#include <list>

#include <criterion/criterion.h>

#include <villas/log.hpp>
#include <villas/utils.hpp>
#include <villas/memory.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/rtds.hpp>
#include <villas/fpga/ips/switch.hpp>

#include <chrono>
#include <villas/utils.hpp>

#include "global.hpp"

using namespace villas::fpga::ip;


Test(fpga, rtds, .description = "RTDS")
{
	auto logger = villas::logging.get("unit-test:rtds");

	std::list<villas::fpga::ip::Rtds*> rtdsIps;
	std::list<villas::fpga::ip::Dma*> dmaIps;

	for (auto& ip : state.cards.front()->ips) {
		if (*ip == villas::fpga::Vlnv("acs.eonerc.rwth-aachen.de:user:rtds_axis:")) {
			auto rtds = reinterpret_cast<villas::fpga::ip::Rtds*>(ip.get());
			rtdsIps.push_back(rtds);
		}

		if (*ip == villas::fpga::Vlnv("xilinx.com:ip:axi_dma:")) {
			auto dma = reinterpret_cast<villas::fpga::ip::Dma*>(ip.get());
			dmaIps.push_back(dma);
		}
	}

	cr_assert(rtdsIps.size() > 0, "No RTDS IPs available to test");
	cr_assert(dmaIps.size() > 0, "No DMA IPs available to test RTDS with");


	for (auto rtds : rtdsIps) {
		for (auto dma : dmaIps) {
			logger->info("Testing {} with DMA {}", *rtds, *dma);

			rtds->dump();

			auto rtdsMaster = rtds->getMasterPort(rtds->masterPort);
			auto rtdsSlave = rtds->getSlavePort(rtds->slavePort);

			auto dmaMaster = dma->getMasterPort(dma->mm2sPort);
			auto dmaSlave = dma->getSlavePort(dma->s2mmPort);

//			rtds->connect(*rtds);
//			logger->info("loopback");
//			while (1);

//			rtds->connect(rtdsMaster, dmaSlave);
//			dma->connect(dmaMaster, rtdsSlave);

			auto mem = villas::HostRam::getAllocator().allocate<int32_t>(0x100 / sizeof(int32_t));


//			auto start = std::chrono::high_resolution_clock::now();

			for (int i = 1; i < 5; i++) {
				logger->info("RTT iteration {}", i);

//				logger->info("Prepare read");
				cr_assert(dma->read(mem.getMemoryBlock(), mem.getMemoryBlock().getSize()),
				          "Failed to initiate DMA read");

//				logger->info("Wait read");
				const size_t bytesRead = dma->readComplete();
				cr_assert(bytesRead > 0,
				          "Failed to complete DMA read");

//				logger->info("Bytes received: {}", bytesRead);
//				logger->info("Prepare write");
				cr_assert(dma->write(mem.getMemoryBlock(), bytesRead),
				          "Failed to initiate DMA write");

//				logger->info("Wait write");
//				const size_t bytesWritten = dma->writeComplete();
//				cr_assert(bytesWritten > 0,
//				          "Failed to complete DMA write");

//				usleep(5);
//				sched_yield();

//				for (int i = 0;)
//				rdtsc_sleep();

//				static constexpr int loopCount = 10000;
//				if (i % loopCount == 0) {
//					const auto end = std::chrono::high_resolution_clock::now();

//					auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start) / loopCount;

//					logger->info("Avg. loop duration: {} us", durationUs.count());

//					start = std::chrono::high_resolution_clock::now();
//				}
			}

			logger->info(CLR_GRN("Passed"));
		}
	}
}
