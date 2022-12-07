/** Streaming data from STDIN/OUT to FPGA.
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2022, Steffen Vogel
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
#if 1
		aurora_channels[2]->connect(aurora_channels[2]->getDefaultMasterPort(), dma->getDefaultSlavePort());
		dma->connect(dma->getDefaultMasterPort(), aurora_channels[2]->getDefaultSlavePort());
#else
		dma->connectLoopback();
#endif
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
				float fval = *((float*)(&ival));
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
