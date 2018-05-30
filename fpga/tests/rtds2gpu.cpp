/** FIFO unit test.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
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

#include <criterion/criterion.h>

#include <villas/log.hpp>
#include <villas/memory.hpp>
#include <villas/fpga/card.hpp>

#include <villas/fpga/ips/rtds2gpu.hpp>
#include <villas/fpga/ips/switch.hpp>
#include <villas/fpga/ips/dma.hpp>

#include "global.hpp"


static constexpr size_t SAMPLE_SIZE		= 4;
static constexpr size_t SAMPLE_COUNT	= 16;
static constexpr size_t FRAME_SIZE		= SAMPLE_COUNT * SAMPLE_SIZE;

static constexpr size_t DOORBELL_OFFSET = SAMPLE_COUNT;
static constexpr size_t DATA_OFFSET = 0;


Test(fpga, rtds2gpu, .description = "Rtds2Gpu")
{
	auto logger = loggerGetOrCreate("unittest:rtds2gpu");

	for(auto& ip : state.cards.front()->ips) {
		if(*ip != villas::fpga::Vlnv("acs.eonerc.rwth-aachen.de:hls:rtds2gpu:"))
			continue;

		logger->info("Testing {}", *ip);


		/* Collect neccessary IPs */

		auto rtds2gpu = reinterpret_cast<villas::fpga::ip::Rtds2Gpu&>(*ip);

		auto axiSwitch = reinterpret_cast<villas::fpga::ip::AxiStreamSwitch*>(
		                     state.cards.front()->lookupIp(villas::fpga::Vlnv("xilinx.com:ip:axis_switch:")));

		auto dma = reinterpret_cast<villas::fpga::ip::Dma*>(
		                     state.cards.front()->lookupIp(villas::fpga::Vlnv("xilinx.com:ip:axi_dma:")));

		rtds2gpu.dump(spdlog::level::debug);

		cr_assert_not_null(axiSwitch, "No AXI switch IP found");
		cr_assert_not_null(dma, "No DMA IP found");


		/* Allocate and prepare memory */

		// allocate space for all samples and doorbell register
		auto dmaMemSrc = villas::HostDmaRam::getAllocator(0).allocate<uint32_t>(SAMPLE_COUNT + 1);
		auto dmaMemDst = villas::HostDmaRam::getAllocator(0).allocate<uint32_t>(SAMPLE_COUNT + 1);

		memset(&dmaMemSrc, 0x11, dmaMemSrc.getMemoryBlock().getSize());
		memset(&dmaMemDst, 0x55, dmaMemDst.getMemoryBlock().getSize());

		const uint32_t* dataSrc = &dmaMemSrc[DATA_OFFSET];
		const uint32_t* dataDst = &dmaMemDst[DATA_OFFSET];

		// connect DMA to Rtds2Gpu IP
		// TODO: this should be done automatically
		cr_assert(axiSwitch->connect(6, 7));

		cr_assert(rtds2gpu.startOnce(dmaMemDst.getMemoryBlock(), SAMPLE_COUNT, DATA_OFFSET*4, DOORBELL_OFFSET*4),
		          "Preparing Rtds2Gpu IP failed");

		cr_assert(dma->write(dmaMemSrc.getMemoryBlock(), FRAME_SIZE),
		          "Starting DMA MM2S transfer failed");

		cr_assert(dma->writeComplete(),
		          "DMA failed");

		while(not rtds2gpu.isFinished());

		const uint32_t* doorbellDst = &dmaMemDst[DOORBELL_OFFSET];
		rtds2gpu.dump(spdlog::level::info);
		rtds2gpu.dumpDoorbell(*doorbellDst);

		cr_assert(memcmp(dataSrc, dataDst, FRAME_SIZE) == 0, "Memory not equal");

		logger->info(TXT_GREEN("Passed"));
	}
}
