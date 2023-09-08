/* Streaming data from STDIN/OUT to FPGA.
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * Author: Niklas Eiling <niklas.eiling@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2022-2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <csignal>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <jansson.h>
#include <thread>

#include <CLI11.hpp>
#include <rang.hpp>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>

#include <villas/fpga/core.hpp>
#include <villas/fpga/card.hpp>
#include <villas/fpga/vlnv.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/rtds.hpp>
#include <villas/fpga/ips/aurora_xilinx.hpp>
#include <villas/fpga/utils.hpp>

using namespace villas;

static std::shared_ptr<kernel::pci::DeviceList> pciDevices;
static auto logger = villas::logging.get("ctrl");

void writeToDmaFromStdIn(std::shared_ptr<villas::fpga::ip::Dma> dma)
{
	auto &alloc = villas::HostRam::getAllocator();
	const std::shared_ptr<villas::MemoryBlock> block = alloc.allocateBlock(0x200 * sizeof(float));
	villas::MemoryAccessor<float> mem = *block;
	dma->makeAccesibleFromVA(block);

	logger->info("Please enter values to write to the device, separated by ';'");

	while (true) {
		// Read values from stdin
		std::string line;
		std::getline(std::cin, line);
		auto values = villas::utils::tokenize(line, ";");

		size_t i = 0;
		for (auto &value: values) {
			if (value.empty()) continue;

			const float number = std::stof(value);
			mem[i++] = number;
		}

		// Initiate write transfer
		bool state = dma->write(*block, i * sizeof(float));
		if (!state)
			logger->error("Failed to write to device");

		auto writeComp = dma->writeComplete();
		logger->debug("Wrote {} bytes", writeComp.bytes);
	}
	// auto &alloc = villas::HostRam::getAllocator();

	// const std::shared_ptr<villas::MemoryBlock> block[] = {
	// 	alloc.allocateBlock(0x200 * sizeof(uint32_t)),
	// 	alloc.allocateBlock(0x200 * sizeof(uint32_t))
	// };
	// villas::MemoryAccessor<int32_t> mem[] = {*block[0], *block[1]};

	// for (auto b : block) {
	// 	dma->makeAccesibleFromVA(b);
	// }

	// size_t cur = 0, next = 1;
	// std::ios::sync_with_stdio(false);
	// std::string line;
	// bool firstXfer = true;

	// while(true) {
	// 	// Read values from stdin

	// 	std::getline(std::cin, line);
	// 	auto values = villas::utils::tokenize(line, ";");

	// 	size_t i = 0;
	// 	for (auto &value: values) {
	// 		if (value.empty()) continue;

	// 		const float number = std::stof(value);
	// 		mem[cur][i++] = number;
	// 	}

	// 	// Initiate write transfer
	// 	bool state = dma->write(*block[cur], i * sizeof(float));
	// 	if (!state)
	// 		logger->error("Failed to write to device");

	// 	if (!firstXfer) {
	// 		auto bytesWritten = dma->writeComplete();
	// 		logger->debug("Wrote {} bytes", bytesWritten.bytes);
	// 	} else {
	// 		firstXfer = false;
	// 	}

	// 	cur = next;
	// 	next = (next + 1) % (sizeof(mem) / sizeof(mem[0]));
	// }
}

void readFromDmaToStdOut(std::shared_ptr<villas::fpga::ip::Dma> dma,
	std::unique_ptr<fpga::BufferedSampleFormatter> formatter)
{
	auto &alloc = villas::HostRam::getAllocator();

	const std::shared_ptr<villas::MemoryBlock> block[] = {
		alloc.allocateBlock(0x200 * sizeof(uint32_t)),
		alloc.allocateBlock(0x200 * sizeof(uint32_t))
	};
	villas::MemoryAccessor<int32_t> mem[] = {*block[0], *block[1]};

	for (auto b : block) {
		dma->makeAccesibleFromVA(b);
	}

	size_t cur = 0, next = 1;
	std::ios::sync_with_stdio(false);

	// Setup read transfer
	dma->read(*block[0], block[0]->getSize());

	while (true) {
		logger->trace("Read from stream and write to address {}:{:p}", block[next]->getAddrSpaceId(), block[next]->getOffset());
		// We could use the number of interrupts to determine if we missed a chunk of data
		dma->read(*block[next], block[next]->getSize());
		auto c = dma->readComplete();

		if (c.interrupts > 1) {
			logger->warn("Missed {} interrupts", c.interrupts - 1);
		}

		logger->debug("bytes: {}, intrs: {}, bds: {}",
			c.bytes, c.interrupts, c.bds);
		try {
			for (size_t  i = 0; i*4 < c.bytes; i++) {
				int32_t ival = mem[cur][i];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
				float fval = *((float*)(&ival)); // cppcheck-suppress invalidPointerCast
#pragma GCC diagnostic pop
				formatter->format(fval);
				printf("%#x\n", ival);
			}
			formatter->output(std::cout);
		} catch (const std::exception &e) {
			logger->warn("Failed to output data: {}", e.what());
		}

		cur = next;
		next = (next + 1) % (sizeof(mem) / sizeof(mem[0]));
	}
}

int main(int argc, char* argv[])
{
	// Command Line Parser
	CLI::App app{"VILLASfpga data output"};

	try {
		std::string configFile;
		app.add_option("-c,--config", configFile, "Configuration file")
				->check(CLI::ExistingFile);

		std::string fpgaName = "vc707";
		app.add_option("--fpga", fpgaName, "Which FPGA to use");
		std::string connectStr = "";
		app.add_option("-x,--connect", connectStr, "Connect a FPGA port with another or stdin/stdout");
		bool noDma = false;
		app.add_flag("--no-dma", noDma, "Do not setup DMA, only setup FPGA and Crossbar links");
		std::string outputFormat = "short";
		app.add_option("--output-format", outputFormat, "Output format (short, long)");
		bool dumpGraph = false;
		app.add_flag("--dump-graph", dumpGraph, "Dumps the graph of memory regions into \"graph.dot\"");
		bool dumpAuroraChannels = true;
		app.add_flag("--dump-aurora", dumpAuroraChannels, "Dumps the detected Aurora channels.");
		app.parse(argc, argv);

		// Logging setup

		logging.setLevel(spdlog::level::debug);
		fpga::setupColorHandling();

		if (configFile.empty()) {
			logger->error("No configuration file provided/ Please use -c/--config argument");
			return 1;
		}

		auto card = fpga::setupFpgaCard(configFile, fpgaName);

		std::vector<std::shared_ptr<fpga::ip::AuroraXilinx>> aurora_channels;
		for (int i = 0; i < 4; i++) {
			auto name = fmt::format("aurora_8b10b_ch{}", i);
			auto id = fpga::ip::IpIdentifier("xilinx.com:ip:aurora_8b10b:", name);
			auto aurora = std::dynamic_pointer_cast<fpga::ip::AuroraXilinx>(card->lookupIp(id));
			if (aurora == nullptr) {
				logger->error("No Aurora interface found on FPGA");
				return 1;
			}

			aurora_channels.push_back(aurora);
		}

		auto dma = std::dynamic_pointer_cast<fpga::ip::Dma>
					(card->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_dma:")));
		if (dma == nullptr) {
			logger->error("No DMA found on FPGA ");
			return 1;
		}

		if (dumpGraph) {
			auto &mm = MemoryManager::get();
			mm.getGraph().dump("graph.dot");
		}

		if (dumpAuroraChannels) {
			for (auto aurora : aurora_channels)
				aurora->dump();
		}
		// Configure Crossbar switch
		const fpga::ConnectString parsedConnectString(connectStr);
		parsedConnectString.configCrossBar(dma, aurora_channels);

		std::unique_ptr<std::thread> stdInThread = nullptr;
		if (!noDma && parsedConnectString.isDstStdout()) {
			auto formatter = fpga::getBufferedSampleFormatter(outputFormat, 16);
			// We copy the dma shared ptr but move the fomatter unqiue ptr as we don't need it
			// in this thread anymore
			stdInThread = std::make_unique<std::thread>(readFromDmaToStdOut, dma, std::move(formatter));
		}
		if (!noDma && parsedConnectString.isSrcStdin()) {
			writeToDmaFromStdIn(dma);
		}

		if (stdInThread) {
			stdInThread->join();
		}
	} catch (const RuntimeError &e) {
		logger->error("Error: {}", e.what());
		return -1;
	} catch (const CLI::ParseError &e) {
		return app.exit(e);
	} catch (const std::exception &e) {
		logger->error("Error: {}", e.what());
		return -1;
	} catch (...) {
		logger->error("Unknown error");
		return -1;
	}

	return 0;
}
