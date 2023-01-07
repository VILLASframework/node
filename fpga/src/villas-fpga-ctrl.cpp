/** Streaming data from STDIN/OUT to FPGA.
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * Author: Niklas Eiling <niklas.eiling@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2017-2022, Steffen Vogel
 * SPDX-FileCopyrightText: 2022-2023, Niklas Eiling
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
#include <villas/fpga/fpgaHelper.hpp>

using namespace villas;

static std::shared_ptr<kernel::pci::DeviceList> pciDevices;
static auto logger = villas::logging.get("cat");



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

		app.parse(argc, argv);

		// Logging setup

		logging.setLevel(spdlog::level::debug);
		fpga::setupColorHandling();

		if (configFile.empty()) {
			logger->error("No configuration file provided/ Please use -c/--config argument");
			return 1;
		}

		//FIXME: This must be called before card is intialized, because the card descructor
		// 	 still accesses the allocated memory. This order ensures that the allocator
		//       is destroyed AFTER the card.
		auto &alloc = villas::HostRam::getAllocator();
		villas::MemoryAccessor<int32_t> mem[] = {alloc.allocate<int32_t>(0x200), alloc.allocate<int32_t>(0x200)};
		const villas::MemoryBlock block[] = {mem[0].getMemoryBlock(), mem[1].getMemoryBlock()};

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
		fpga::configCrossBarUsingConnectString(connectStr, dma, aurora_channels);

		if (!noDma) {
			for (auto b : block) {
				dma->makeAccesibleFromVA(b);
			}

			auto &mm = MemoryManager::get();
			mm.getGraph().dump("graph.dot");

			// Setup read transfer
			dma->read(block[0], block[0].getSize());
			size_t cur = 0, next = 1;
			while (true) {
				dma->read(block[next], block[next].getSize());
				auto bytesRead = dma->readComplete();
				// Setup read transfer

				//auto valuesRead = bytesRead / sizeof(int32_t);
				//logger->info("Read {} bytes", bytesRead);

				//for (size_t i = 0; i < valuesRead; i++)
				//	std::cerr << std::hex << mem[i] << ";";
				//std::cerr << std::endl;

				for (size_t i = 0; i*4 < bytesRead; i++) {
					int32_t ival = mem[cur][i];
					float fval = *((float*)(&ival)); // cppcheck-suppress invalidPointerCast
					//std::cerr << std::hex << ival << ",";
					std::cerr << fval << std::endl;
					/*int64_t ival = (int64_t)(mem[1] & 0xFFFF) << 48 |
						(int64_t)(mem[1] & 0xFFFF0000) << 16 |
						(int64_t)(mem[0] & 0xFFFF) << 16 |
						(int64_t)(mem[0] & 0xFFFF0000) >> 16;
					double dval = *((double*)(&ival));
					std::cerr << std::hex << ival << "," << dval << std::endl;
					bytesRead -= 8;*/
					//logger->info("Read value: {}", dval);
				}
				cur = next;
				next = (next + 1) % (sizeof(mem)/sizeof(mem[0]));
			}
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
