/** Helper function for directly using VILLASfpga outside of VILLASnode
 *
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @copyright 2022, Steffen Vogel, Niklas Eiling
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
static auto logger = villas::logging.get("streamer");

void fpga::setupColorHandling()
{
	// Handle Control-C nicely
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = [](int){
		std::cout << std::endl << rang::style::reset << rang::fgB::red;
		std::cout << "Control-C detected, exiting..." << rang::style::reset << std::endl;
		std::exit(1); // Will call the correct exit func, no unwinding of the stack though
	};

	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, nullptr);

	// Reset color if exiting not by signal
	std::atexit([](){
		std::cout << rang::style::reset;
	});
}

std::shared_ptr<fpga::PCIeCard>
fpga::setupFpgaCard(const std::string &configFile, const std::string &fpgaName)
{
	pciDevices = std::make_shared<kernel::pci::DeviceList>();

	auto vfioContainer = std::make_shared<kernel::vfio::Container>();

	// Parse FPGA configuration
	FILE* f = fopen(configFile.c_str(), "r");
	if (!f)
		throw RuntimeError("Cannot open config file: {}", configFile);

	json_t* json = json_loadf(f, 0, nullptr);
	if (!json) {
		logger->error("Cannot parse JSON config");
		fclose(f);
		throw RuntimeError("Cannot parse JSON config");
	}

	fclose(f);

	json_t* fpgas = json_object_get(json, "fpgas");
	if (fpgas == nullptr) {
		logger->error("No section 'fpgas' found in config");
		exit(1);
	}

	// Get the FPGA card plugin
	auto fpgaCardFactory = plugin::registry->lookup<fpga::PCIeCardFactory>("pcie");
	if (fpgaCardFactory == nullptr) {
		logger->error("No FPGA plugin found");
		exit(1);
	}

	// Create all FPGA card instances using the corresponding plugin
	auto cards = fpgaCardFactory->make(fpgas, pciDevices, vfioContainer);

	std::shared_ptr<fpga::PCIeCard> card;
	for (auto &fpgaCard : cards) {
		if (fpgaCard->name == fpgaName) {
			return fpgaCard;
		}
	}

	// Deallocate JSON config
	json_decref(json);

	if (!card)
		throw RuntimeError("FPGA card {} not found in config or not working", fpgaName);

	return card;
}

