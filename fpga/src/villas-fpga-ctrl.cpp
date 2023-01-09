/** Streaming data from STDIN/OUT to FPGA.
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * Author: Niklas Eiling <niklas.eiling@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2022 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#include <csignal>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <jansson.h>

#include <CLI11.hpp>
#include <rang.hpp>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>
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

	auto &mm = MemoryManager::get();
	mm.getGraph().dump("graph.dot");


	size_t cur = 0, next = 1;
	std::ios::sync_with_stdio(false);
	size_t bytesRead;

	// Setup read transfer
	dma->read(*block[0], block[0]->getSize());

	while (true) {
		logger->trace("Read from stream and write to address {}:{:p}", block[next]->getAddrSpaceId(), block[next]->getOffset());
		// We could use the number of interrupts to determine if we missed a chunk of data
		dma->read(*block[next], block[next]->getSize());
		bytesRead = dma->readComplete();

		for (size_t i = 0; i*4 < bytesRead; i++) {
			int32_t ival = mem[cur][i];
			float fval = *((float*)(&ival)); // cppcheck-suppress invalidPointerCast
			formatter->format(fval);
		}
		formatter->output(std::cout);

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

		for (auto aurora : aurora_channels)
			aurora->dump();

		// Configure Crossbar switch
		const fpga::ConnectString parsedConnectString(connectStr);
		parsedConnectString.configCrossBar(dma, aurora_channels);

		if (!noDma) {
			auto formatter = fpga::getBufferedSampleFormatter(outputFormat, 16);
			readFromDmaToStdOut(std::move(dma), std::move(formatter));
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
