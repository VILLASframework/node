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
#include <filesystem>

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

fpga::ConnectString::ConnectString(std::string& connectString, int maxPortNum) :
	log(villas::logging.get("ConnectString")),
	maxPortNum(maxPortNum),
	bidirectional(false),
	invert(false),
	srcAsInt(-1),
	dstAsInt(-1),
	srcIsStdin(false),
	dstIsStdout(false),
	dmaLoopback(false)
{
	parseString(connectString);
}

void fpga::ConnectString::parseString(std::string& connectString)
{
	if (connectString.empty())
		return;

	if (connectString == "loopback") {
		logger->info("Connecting loopback");
		srcIsStdin = true;
		dstIsStdout = true;
		bidirectional = true;
		dmaLoopback = true;
		return;
	}

	static const std::regex regex("([0-9]+)([<\\->]+)([0-9]+|stdin|stdout|pipe)");
	std::smatch match;

	if (!std::regex_match(connectString, match, regex) || match.size() != 4) {
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

	srcAsInt = portStringToInt(srcStr);
	dstAsInt = portStringToInt(dstStr);
	if (srcAsInt == -1) {
		srcIsStdin = true;
		dstIsStdout = bidirectional;
	}
	if (dstAsInt == -1) {
		dstIsStdout = true;
		srcIsStdin = bidirectional;
	}
}

int fpga::ConnectString::portStringToInt(std::string &str) const
{
	if (str == "stdin" || str == "stdout" || str == "pipe") {
		return -1;
	} else {
		const int port = std::stoi(str);

		if (port > maxPortNum || port < 0)
			throw std::runtime_error("Invalid port number");

		return port;
	}
}


// parses a string like "1->2" or "1<->stdout" and configures the crossbar accordingly
void fpga::ConnectString::configCrossBar(std::shared_ptr<villas::fpga::ip::Dma> dma,
	std::vector<std::shared_ptr<fpga::ip::AuroraXilinx>>& aurora_channels) const
{
	if (dmaLoopback) {
		log->info("Configuring DMA loopback");
		dma->connectLoopback();
		return;
	}

	log->info("Connecting {} to {}, {}directional",
		(srcAsInt==-1 ? "stdin" : std::to_string(srcAsInt)),
		(dstAsInt==-1 ? "stdout" : std::to_string(dstAsInt)),
		(bidirectional ? "bi" : "uni"));

	std::shared_ptr<fpga::ip::Node> src;
	std::shared_ptr<fpga::ip::Node> dest;
	if (srcIsStdin) {
		src = dma;
	} else {
		src = aurora_channels[srcAsInt];
	}

	if (dstIsStdout) {
		dest = dma;
	} else {
		dest = aurora_channels[dstAsInt];
	}

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
	auto configDir = std::filesystem::path(configFile).parent_path();

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
	auto cards = fpgaCardFactory->make(fpgas, pciDevices, vfioContainer, configDir);

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

std::unique_ptr<fpga::BufferedSampleFormatter> fpga::getBufferedSampleFormatter(
	const std::string &format,
	size_t bufSizeInSamples)
{
	if (format == "long") {
		return std::make_unique<fpga::BufferedSampleFormatterLong>(bufSizeInSamples);
	} else if (format == "short") {
		return std::make_unique<fpga::BufferedSampleFormatterShort>(bufSizeInSamples);
	} else {
		throw RuntimeError("Unknown output format '{}'", format);
	}
}
