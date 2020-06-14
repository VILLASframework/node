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

#include <iostream>

#include <villas/log.hpp>
#include <villas/memory.hpp>
#include <villas/fpga/card.hpp>

#include <villas/fpga/ips/rtds2gpu.hpp>
#include <villas/fpga/ips/gpu2rtds.hpp>
#include <villas/fpga/ips/switch.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/rtds.hpp>
#include <villas/gpu.hpp>

#include "global.hpp"

using namespace villas;


static constexpr size_t SAMPLE_SIZE		= 4;
static constexpr size_t SAMPLE_COUNT	= 1;
static constexpr size_t FRAME_SIZE		= SAMPLE_COUNT * SAMPLE_SIZE;

static constexpr size_t DOORBELL_OFFSET = SAMPLE_COUNT;
static constexpr size_t DATA_OFFSET = 0;

static void dumpMem(const uint32_t* addr, size_t len)
{
	const size_t bytesPerLine = 16;
	const size_t lines = (len) / bytesPerLine + 1;
	const uint8_t* buf = reinterpret_cast<const uint8_t*>(addr);

	size_t bytesRead = 0;

	for (size_t line = 0; line < lines; line++) {
		const unsigned base = line * bytesPerLine;
		printf("0x%04x: ", base);

		for (size_t i = 0; i < bytesPerLine && bytesRead < len; i++) {
			printf("0x%02x ", buf[base + i]);
			bytesRead++;
		}
		puts("");
	}
}

Test(fpga, rtds2gpu_loopback_dma, .description = "Rtds2Gpu")
{
	auto logger = logging.get("unit-test:rtds2gpu");

	for (auto &ip : state.cards.front()->ips) {
		if (*ip != fpga::Vlnv("acs.eonerc.rwth-aachen.de:hls:rtds2gpu:"))
			continue;

		logger->info("Testing {}", *ip);


		/* Collect neccessary IPs */

		auto rtds2gpu = std::dynamic_pointer_cast<fpga::ip::Rtds2Gpu>(ip);

		auto axiSwitch = std::dynamic_pointer_cast<fpga::ip::AxiStreamSwitch>(
		                     state.cards.front()->lookupIp(fpga::Vlnv("xilinx.com:ip:axis_switch:")));

		auto dma = std::dynamic_pointer_cast<fpga::ip::Dma>(
		                     state.cards.front()->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_dma:")));

		auto gpu2rtds = std::dynamic_pointer_cast<fpga::ip::Gpu2Rtds>(
		                     state.cards.front()->lookupIp(fpga::Vlnv("acs.eonerc.rwth-aachen.de:hls:gpu2rtds:")));

		auto rtds = std::dynamic_pointer_cast<fpga::ip::Rtds>(
		                     state.cards.front()->lookupIp(fpga::Vlnv("acs.eonerc.rwth-aachen.de:user:rtds_axis:")));


		cr_assert_not_null(axiSwitch, "No AXI switch IP found");
		cr_assert_not_null(dma, "No DMA IP found");
		cr_assert_not_null(gpu2rtds, "No Gpu2Rtds IP found");
		cr_assert_not_null(rtds, "RTDS IP not found");

		rtds2gpu.dump(spdlog::level::debug);
		gpu2rtds->dump(spdlog::level::debug);


		/* Allocate and prepare memory */

		// allocate space for all samples and doorbell register
		auto dmaMemSrc = HostDmaRam::getAllocator(0).allocate<uint32_t>(SAMPLE_COUNT + 1);
		auto dmaMemDst = HostDmaRam::getAllocator(0).allocate<uint32_t>(SAMPLE_COUNT + 1);
		auto dmaMemDst2 = HostDmaRam::getAllocator(0).allocate<uint32_t>(SAMPLE_COUNT + 1);


		memset(&dmaMemSrc, 0x11, dmaMemSrc.getMemoryBlock().getSize());
		memset(&dmaMemDst, 0x55, dmaMemDst.getMemoryBlock().getSize());
		memset(&dmaMemDst2, 0x77, dmaMemDst2.getMemoryBlock().getSize());

		const uint32_t* dataSrc = &dmaMemSrc[DATA_OFFSET];
		const uint32_t* dataDst = &dmaMemDst[DATA_OFFSET];
		const uint32_t* dataDst2 = &dmaMemDst2[0];

		dumpMem(dataSrc, dmaMemSrc.getMemoryBlock().getSize());
		dumpMem(dataDst, dmaMemDst.getMemoryBlock().getSize());
		dumpMem(dataDst2, dmaMemDst2.getMemoryBlock().getSize());


		// connect AXI Stream from DMA to Rtds2Gpu IP
		cr_assert(dma->connect(rtds2gpu));

		cr_assert(rtds2gpu.startOnce(dmaMemDst.getMemoryBlock(), SAMPLE_COUNT, DATA_OFFSET * 4, DOORBELL_OFFSET * 4),
		          "Preparing Rtds2Gpu IP failed");

		cr_assert(dma->write(dmaMemSrc.getMemoryBlock(), FRAME_SIZE),
		          "Starting DMA MM2S transfer failed");

		cr_assert(dma->writeComplete(),
		          "DMA failed");

		while (not rtds2gpu.isFinished());

		const uint32_t* doorbellDst = &dmaMemDst[DOORBELL_OFFSET];
		rtds2gpu.dump(spdlog::level::info);
		rtds2gpu.dumpDoorbell(*doorbellDst);

		cr_assert(memcmp(dataSrc, dataDst, FRAME_SIZE) == 0, "Memory not equal");


		for (size_t i = 0; i < SAMPLE_COUNT; i++) {
			gpu2rtds->registerFrames[i] = dmaMemDst[i];
		}


		// connect AXI Stream from Gpu2Rtds IP to DMA
		cr_assert(gpu2rtds->connect(*dma));

		cr_assert(dma->read(dmaMemDst2.getMemoryBlock(), FRAME_SIZE),
		          "Starting DMA S2MM transfer failed");

		cr_assert(gpu2rtds->startOnce(SAMPLE_COUNT),
		          "Preparing Gpu2Rtds IP failed");

		cr_assert(dma->readComplete(),
		          "DMA failed");

		while (not gpu2rtds->isFinished());

		cr_assert(memcmp(dataSrc, dataDst2, FRAME_SIZE) == 0, "Memory not equal");

		dumpMem(dataSrc, dmaMemSrc.getMemoryBlock().getSize());
		dumpMem(dataDst, dmaMemDst.getMemoryBlock().getSize());
		dumpMem(dataDst2, dmaMemDst2.getMemoryBlock().getSize());

		logger->info(CLR_GRN("Passed"));
	}
}

Test(fpga, rtds2gpu_rtt_cpu, .description = "Rtds2Gpu RTT via CPU")
{
	auto logger = logging.get("unit-test:rtds2gpu");

	/* Collect neccessary IPs */

	auto gpu2rtds = std::dynamic_pointer_cast<fpga::ip::Gpu2Rtds>(
	                     state.cards.front()->lookupIp(fpga::Vlnv("acs.eonerc.rwth-aachen.de:hls:gpu2rtds:")));

	auto rtds2gpu = std::dynamic_pointer_cast<fpga::ip::Rtds2Gpu>(
	                     state.cards.front()->lookupIp(fpga::Vlnv("acs.eonerc.rwth-aachen.de:hls:rtds2gpu:")));

	cr_assert_not_null(gpu2rtds, "No Gpu2Rtds IP found");
	cr_assert_not_null(rtds2gpu, "No Rtds2Gpu IP not found");

	for (auto &ip : state.cards.front()->ips) {
		if (*ip != fpga::Vlnv("acs.eonerc.rwth-aachen.de:user:rtds_axis:"))
			continue;

		auto &rtds = dynamic_cast<fpga::ip::Rtds&>(*ip);
		logger->info("Testing {}", rtds);

		auto dmaRam = HostDmaRam::getAllocator().allocate<uint32_t>(SAMPLE_COUNT + 1);
		uint32_t* data = &dmaRam[DATA_OFFSET];
		uint32_t* doorbell = &dmaRam[DOORBELL_OFFSET];

		// TEST: rtds loopback via switch, this should always work and have RTT=1
		//cr_assert(rtds.connect(rtds));
		//logger->info("loopback");
		//while (1);

		cr_assert(rtds.connect(*rtds2gpu));
		cr_assert(gpu2rtds->connect(rtds));


		for (size_t i = 1; i <= 10000; ) {
			rtds2gpu->doorbellReset(*doorbell);
			rtds2gpu->startOnce(dmaRam.getMemoryBlock(), SAMPLE_COUNT, DATA_OFFSET * 4, DOORBELL_OFFSET * 4);

			// Wait by polling rtds2gpu IP or ...
			// while (not rtds2gpu->isFinished());

			// Wait by polling (local) doorbell register (= just memory)
			while (not rtds2gpu->doorbellIsValid(*doorbell));


			// copy samples to gpu2rtds IP
			for (size_t i = 0; i < SAMPLE_COUNT; i++) {
				gpu2rtds->registerFrames[i] = data[i];
			}

			// Waiting for gpu2rtds is not strictly required
			gpu2rtds->startOnce(SAMPLE_COUNT);
			//while (not gpu2rtds->isFinished());

			if (i % 1000 == 0) {
				logger->info("Successful iterations {}, data {}", i, data[0]);
				rtds2gpu->dump();
				rtds2gpu->dumpDoorbell(data[1]);
			}
		}

		logger->info(CLR_GRN("Passed"));
	}
}

void gpu_rtds_rtt_start(volatile uint32_t* dataIn, volatile reg_doorbell_t* doorbellIn,
                        volatile uint32_t* dataOut, volatile fpga::ip::ControlRegister* controlRegister);

void gpu_rtds_rtt_stop();

Test(fpga, rtds2gpu_rtt_gpu, .description = "Rtds2Gpu RTT via GPU")
{
	auto logger = logging.get("unit-test:rtds2gpu");

	/* Collect neccessary IPs */

	auto gpu2rtds = std::dynamic_pointer_cast<fpga::ip::Gpu2Rtds>(
	                     state.cards.front()->lookupIp(fpga::Vlnv("acs.eonerc.rwth-aachen.de:hls:gpu2rtds:")));

	auto rtds2gpu = std::dynamic_pointer_cast<fpga::ip::Rtds2Gpu>(
	                     state.cards.front()->lookupIp(fpga::Vlnv("acs.eonerc.rwth-aachen.de:hls:rtds2gpu:")));

	cr_assert_not_null(gpu2rtds, "No Gpu2Rtds IP found");
	cr_assert_not_null(rtds2gpu, "No Rtds2Gpu IP not found");

	auto gpuPlugin = Registry::lookup<GpuFactory>("cuda");
	cr_assert_not_null(gpuPlugin, "No GPU plugin found");

	auto gpus = gpuPlugin->make();
	cr_assert(gpus.size() > 0, "No GPUs found");

	// just get first cpu
	auto &gpu = gpus.front();

	// allocate memory on GPU and make accessible by to PCIe/FPGA
	auto gpuRam = gpu->getAllocator().allocate<uint32_t>(SAMPLE_COUNT + 1);
	cr_assert(gpu->makeAccessibleToPCIeAndVA(gpuRam.getMemoryBlock()));

	// make Gpu2Rtds IP register memory on FPGA accessible to GPU
	cr_assert(gpu->makeAccessibleFromPCIeOrHostRam(gpu2rtds->getRegisterMemory()));

	auto tr = gpu->translate(gpuRam.getMemoryBlock());

	auto dataIn		= reinterpret_cast<uint32_t*>(tr.getLocalAddr(DATA_OFFSET * sizeof(uint32_t)));
	auto doorbellIn	= reinterpret_cast<reg_doorbell_t*>(tr.getLocalAddr(DOORBELL_OFFSET * sizeof(uint32_t)));


	auto gpu2rtdsRegisters = gpu->translate(gpu2rtds->getRegisterMemory());

	auto frameRegister		= reinterpret_cast<uint32_t*>(gpu2rtdsRegisters.getLocalAddr(gpu2rtds->registerFrameOffset));
	auto controlRegister	= reinterpret_cast<fpga::ip::ControlRegister*>(gpu2rtdsRegisters.getLocalAddr(gpu2rtds->registerControlAddr));

//	auto doorbellInCpu = reinterpret_cast<reg_doorbell_t*>(&gpuRam[DOORBELL_OFFSET]);

	for (auto &ip : state.cards.front()->ips) {
		if (*ip != fpga::Vlnv("acs.eonerc.rwth-aachen.de:user:rtds_axis:"))
			continue;

		auto &rtds = dynamic_cast<fpga::ip::Rtds&>(*ip);
		logger->info("Testing {}", rtds);


		// TEST: rtds loopback via switch, this should always work and have RTT=1
		//cr_assert(rtds.connect(rtds));
		//logger->info("loopback");
		//while (1);

		cr_assert(rtds.connect(*rtds2gpu));
		cr_assert(gpu2rtds->connect(rtds));

		// launch once so they are configured
		cr_assert(rtds2gpu->startOnce(gpuRam.getMemoryBlock(), SAMPLE_COUNT, DATA_OFFSET * 4, DOORBELL_OFFSET * 4));
		cr_assert(gpu2rtds->startOnce(SAMPLE_COUNT));

		rtds2gpu->setAutoRestart(true);
		rtds2gpu->start();

		logger->info("GPU RTT RTDS");

		std::string dummy;

//		logger->info("Press enter to proceed");
//		std::cin >> dummy;

		gpu_rtds_rtt_start(dataIn, doorbellIn, frameRegister, controlRegister);

//		while (1) {
//			cr_assert(rtds2gpu->startOnce(gpuRam.getMemoryBlock(), SAMPLE_COUNT, DATA_OFFSET * 4, DOORBELL_OFFSET * 4));
//		}

//		for (int i = 0; i < 10000; i++) {
//			while (not doorbellInCpu->is_valid);
//			logger->debug("received data");
//		}

//		logger->info("Press enter to cancel");
//		std::cin >> dummy;

		while (1) {
			sleep(1);
//			logger->debug("Current sequence number: {}", doorbellInCpu->seq_nr);
			logger->debug("Still running");
		}

		gpu_rtds_rtt_stop();



		logger->info(CLR_GRN("Passed"));
	}
}
