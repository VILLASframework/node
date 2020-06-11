/** Streaming data from STDIN/OUT to FPGA.
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
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

#include <csignal>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <jansson.h>

#include <CLI11.hpp>
#include <rang.hpp>

#include <villas/log.hpp>
#include <villas/utils.hpp>
#include <villas/utils.hpp>

#include <villas/fpga/ip.hpp>
#include <villas/fpga/card.hpp>
#include <villas/fpga/vlnv.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/rtds.hpp>
#include <villas/fpga/ips/aurora.hpp>

using namespace villas;

static struct pci pci;
static auto logger = villas::logging.get("streamer");

void setupColorHandling()
{
	// Nice Control-C
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = [](int){
		std::cout << std::endl << rang::style::reset << rang::fgB::red;
		std::cout << "Control-C detected, exiting..." << rang::style::reset << std::endl;
		std::exit(1); // will call the correct exit func, no unwinding of the stack though
	};
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, nullptr);

	// reset color if exiting not by signal
	std::atexit([](){std::cout << rang::style::reset;});
}

std::shared_ptr<fpga::PCIeCard>
setupFpgaCard(const std::string& configFile, const std::string& fpgaName)
{
	if(pci_init(&pci) != 0) {
		logger->error("Cannot initialize PCI");
		exit(1);
	}

	auto vfioContainer = villas::VfioContainer::create();

	/* Parse FPGA configuration */
	FILE* f = fopen(configFile.c_str(), "r");
	if(f == nullptr) {
		logger->error("Cannot open config file: {}", configFile);
	}

	json_t* json = json_loadf(f, 0, nullptr);
	if(json == nullptr) {
		logger->error("Cannot parse JSON config");
		fclose(f);
		exit(1);
	}

	fclose(f);

	json_t* fpgas = json_object_get(json, "fpgas");
	if(fpgas == nullptr) {
		logger->error("No section 'fpgas' found in config");
		exit(1);
	}

	// get the FPGA card plugin
	auto fpgaCardPlugin = plugin::Registry::lookup<fpga::PCIeCardFactory>("");
	if(fpgaCardPlugin == nullptr) {
		logger->error("No FPGA plugin found");
		exit(1);
	}

	// create all FPGA card instances using the corresponding plugin
	auto cards = fpgaCardPlugin->make(fpgas, &pci, vfioContainer);

	for(auto& fpgaCard : cards) {
		if(fpgaCard->name == fpgaName) {
			return fpgaCard;
		}
	}

	logger->error("FPGA card {} not found in config or not working", fpgaName);

	// deallocate JSON config
//	json_decref(json);

	return std::shared_ptr<villas::fpga::PCIeCard>();
}

int main(int argc, char* argv[])
{
	/* Command Line Parser */

	CLI::App app{"VILLASfpga data streamer"};

	std::string configFile = "../etc/fpga.json";
	app.add_option("-c,--config", configFile, "Configuration file")
	        ->check(CLI::ExistingFile);

	std::string fpgaName = "vc707";
	app.add_option("--fpga", fpgaName, "Which FPGA to use");

	try {
		app.parse(argc, argv);
	} catch (const CLI::ParseError &e) {
		return app.exit(e);
	}

	/* Logging setup */
	spdlog::set_level(spdlog::level::debug);
	setupColorHandling();

	auto card = setupFpgaCard(configFile, fpgaName);

	auto aurora = dynamic_cast<fpga::ip::Aurora*>
	            (card->lookupIp(fpga::Vlnv("acs.eonerc.rwth-aachen.de:user:aurora:")));

	auto dma = dynamic_cast<fpga::ip::Dma*>
	          (card->lookupIp("hier_0_axi_dma_axi_dma_1"));
	

	if(aurora == nullptr) {
		logger->error("No Aurora interface found on FPGA");
		return 1;
	}

	if(dma == nullptr) {
		logger->error("No DMA found on FPGA ");
		return 1;
	}

	aurora->dump();

	aurora->connect(aurora->getMasterPort(aurora->masterPort),
	              dma->getSlavePort(dma->s2mmPort));

	dma->connect(dma->getMasterPort(dma->mm2sPort),
	             aurora->getSlavePort(aurora->slavePort));

	auto &alloc = villas::HostRam::getAllocator();
	auto mem = alloc.allocate<int32_t>(0x100 / sizeof(int32_t));
	auto block = mem.getMemoryBlock();
	
	dma->makeAccesibleFromVA(block);

	auto &mm = MemoryManager::get();
	mm.getMemoryGraph().dump("graph.dot");

	while(true) {
		dma->read(block, block.getSize());
		const size_t bytesRead = dma->readComplete();
		const size_t valuesRead = bytesRead / sizeof(int32_t);

		for(size_t i = 0; i < valuesRead; i++) {
			std::cerr << mem[i] << ";";
		}
		std::cerr << std::endl;

		std::string line;
		std::getline(std::cin, line);
		auto values = villas::utils::tokenize(line, ";");

		size_t memIdx = 0;

		for(auto& value: values) {
			if(value.empty()) continue;

			const int32_t number = std::stoi(value);
			mem[memIdx++] = number;
		}

		bool state = dma->write(block, memIdx * sizeof(int32_t));
		if (!state)
			logger->error("Failed to write to device");
	}

	return 0;
}
