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
#include <villas/fpga/ips/gpu2rtds.hpp>
#include <villas/fpga/ips/switch.hpp>
#include <villas/fpga/ips/dma.hpp>

#include "global.hpp"


static constexpr size_t SAMPLE_SIZE		= 4;
static constexpr size_t SAMPLE_COUNT	= 8;
static constexpr size_t FRAME_SIZE		= SAMPLE_COUNT * SAMPLE_SIZE;

static constexpr size_t DOORBELL_OFFSET = SAMPLE_COUNT;
static constexpr size_t DATA_OFFSET = 0;

static void dumpMem(const uint32_t* addr, size_t len)
{
	const size_t bytesPerLine = 16;
	const size_t lines = (len) / bytesPerLine + 1;
	const uint8_t* buf = reinterpret_cast<const uint8_t*>(addr);

	size_t bytesRead = 0;

	for(size_t line = 0; line < lines; line++) {
		const unsigned base = line * bytesPerLine;
		printf("0x%04x: ", base);

		for(size_t i = 0; i < bytesPerLine && bytesRead < len; i++) {
			printf("0x%02x ", buf[base + i]);
			bytesRead++;
		}
		puts("");
	}
}

Test(fpga, rtds2gpu, .description = "Rtds2Gpu")
{
	auto logger = loggerGetOrCreate("unittest:rtds2gpu");

	for(auto& ip : state.cards.front()->ips) {
		if(*ip != villas::fpga::Vlnv("acs.eonerc.rwth-aachen.de:hls:rtds2gpu:"))
			continue;

		logger->info("Testing {}", *ip);


		/* Collect neccessary IPs */

		auto rtds2gpu = dynamic_cast<villas::fpga::ip::Rtds2Gpu&>(*ip);

		auto axiSwitch = dynamic_cast<villas::fpga::ip::AxiStreamSwitch*>(
		                     state.cards.front()->lookupIp(villas::fpga::Vlnv("xilinx.com:ip:axis_switch:")));

		auto dma = dynamic_cast<villas::fpga::ip::Dma*>(
		                     state.cards.front()->lookupIp(villas::fpga::Vlnv("xilinx.com:ip:axi_dma:")));

		auto gpu2rtds = dynamic_cast<villas::fpga::ip::Gpu2Rtds*>(
		                     state.cards.front()->lookupIp(villas::fpga::Vlnv("acs.eonerc.rwth-aachen.de:hls:gpu2rtds:")));



		cr_assert_not_null(axiSwitch, "No AXI switch IP found");
		cr_assert_not_null(dma, "No DMA IP found");
		cr_assert_not_null(gpu2rtds, "No Gpu2Rtds IP found");

		rtds2gpu.dump(spdlog::level::debug);
		gpu2rtds->dump(spdlog::level::debug);


		/* Allocate and prepare memory */

		// allocate space for all samples and doorbell register
		auto dmaMemSrc = villas::HostDmaRam::getAllocator(0).allocate<uint32_t>(SAMPLE_COUNT + 1);
		auto dmaMemDst = villas::HostDmaRam::getAllocator(0).allocate<uint32_t>(SAMPLE_COUNT + 1);
		auto dmaMemDst2 = villas::HostDmaRam::getAllocator(0).allocate<uint32_t>(SAMPLE_COUNT + 1);


		memset(&dmaMemSrc, 0x11, dmaMemSrc.getMemoryBlock().getSize());
		memset(&dmaMemDst, 0x55, dmaMemDst.getMemoryBlock().getSize());
		memset(&dmaMemDst2, 0x77, dmaMemDst2.getMemoryBlock().getSize());

		const uint32_t* dataSrc = &dmaMemSrc[DATA_OFFSET];
		const uint32_t* dataDst = &dmaMemDst[DATA_OFFSET];
		const uint32_t* dataDst2 = &dmaMemDst2[0];

		dumpMem(dataSrc, dmaMemSrc.getMemoryBlock().getSize());
		dumpMem(dataDst, dmaMemDst.getMemoryBlock().getSize());
		dumpMem(dataDst2, dmaMemDst2.getMemoryBlock().getSize());


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

		(void) dmaMemDst2;
		(void) dataDst2;

		for(size_t i = 0; i < SAMPLE_COUNT; i++) {
			gpu2rtds->registerFrames[i] = dmaMemDst[i];
		}
		cr_assert(axiSwitch->connect(7, 6));

		cr_assert(dma->read(dmaMemDst2.getMemoryBlock(), FRAME_SIZE),
		          "Starting DMA S2MM transfer failed");

		cr_assert(gpu2rtds->startOnce(SAMPLE_COUNT),
		          "Preparing Gpu2Rtds IP failed");

		cr_assert(dma->readComplete(),
		          "DMA failed");

		while(not rtds2gpu.isFinished());

		cr_assert(memcmp(dataSrc, dataDst2, FRAME_SIZE) == 0, "Memory not equal");

		dumpMem(dataSrc, dmaMemSrc.getMemoryBlock().getSize());
		dumpMem(dataDst, dmaMemDst.getMemoryBlock().getSize());
		dumpMem(dataDst2, dmaMemDst2.getMemoryBlock().getSize());

		logger->info(TXT_GREEN("Passed"));
	}
}
