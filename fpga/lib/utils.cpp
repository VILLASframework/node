/** Helper function for directly using VILLASfpga outside of VILLASnode
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2022 Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2022 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#include <csignal>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <jansson.h>
#include <regex>

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
static auto logger = villas::logging.get("streamer");

const std::shared_ptr<villas::fpga::ip::Node> portStringToStreamVertex(std::string &str,
	std::shared_ptr<villas::fpga::ip::Dma> dma,
	std::vector<std::shared_ptr<fpga::ip::AuroraXilinx>>& aurora_channels)
{
	if (str == "stdin" || str == "stdout") {
		 return dma;
	} else {
		int port = std::stoi(str);

		if (port > 7 || port < 0)
			throw std::runtime_error("Invalid port number");

		return aurora_channels[port];
	}
}

// parses a string lik "1->2" or "1<->stdout" and configures the crossbar
void fpga::configCrossBarUsingConnectString(std::string connectString,
	std::shared_ptr<villas::fpga::ip::Dma> dma,
	std::vector<std::shared_ptr<fpga::ip::AuroraXilinx>>& aurora_channels)
{
	bool bidirectional = false;
	bool invert = false;

	if (connectString.empty())
		return;

	if (connectString == "loopback") {
		logger->info("Connecting loopback");
		// is this working?
		dma->connectLoopback();
	}

	static std::regex re("([0-9]+)([<\\->]+)([0-9]+|stdin|stdout)");
	std::smatch match;

	if (!std::regex_match(connectString, match, re)) {
		logger->error("Invalid connect string: {}", connectString);
		throw std::runtime_error("Invalid connect string");
	}
	if (match[2] == "<->") {
		bidirectional = true;
	} else if(match[2] == "<-") {
		invert = true;
	}

	std::string srcStr = (invert ? match[3] : match[1]);
	std::string dstStr = (invert ? match[1] : match[3]);
	logger->info("Connect string {}: Connecting {} to {}, {}directional",
		connectString, srcStr, dstStr,
		(bidirectional ? "bi" : "uni"));
	auto src = portStringToStreamVertex(srcStr, dma, aurora_channels);
	auto dest = portStringToStreamVertex(dstStr, dma, aurora_channels);
	src->connect(src->getDefaultMasterPort(), dest->getDefaultSlavePort());
	if (bidirectional) {
		dest->connect(dest->getDefaultMasterPort(), src->getDefaultSlavePort());
	}
}

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

