/** API for interacting with the FPGA DMA Controller.
 *
 * Author: Niklas Eiling <niklas.eiling@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023 Niklas Eiling <niklas.eiling@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#include <villas/fpga/villasfpga_dma.h>

#include <csignal>
#include <iostream>
#include <string>
#include <stdexcept>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>

#include <villas/fpga/core.hpp>
#include <villas/fpga/card.hpp>
#include <villas/fpga/vlnv.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/utils.hpp>

using namespace villas;

static std::shared_ptr<kernel::pci::DeviceList> pciDevices;
static auto logger = villas::logging.get("villasfpga_dma");

struct villasfpga_handle_t {
	std::shared_ptr<villas::fpga::Card> card;
	std::shared_ptr<villas::fpga::ip::Dma> dma;
};
struct villasfpga_memory_t {
	std::shared_ptr<villas::MemoryBlock> block;
};

villasfpga_handle villasfpga_init(const char *configFile)
{
	std::string fpgaName = "vc707";
	std::string connectStr = "3<->pipe";
	std::string outputFormat = "short";
	bool dumpGraph = false;
	bool dumpAuroraChannels = true;
	try {
		// Logging setup
		logging.setLevel(spdlog::level::debug);
		fpga::setupColorHandling();

		if (configFile == nullptr || configFile[0] == '\0') {
			logger->error("No configuration file provided/ Please use -c/--config argument");
			return nullptr;
		}

		auto handle = new villasfpga_handle_t;
		handle->card = fpga::setupFpgaCard(configFile, fpgaName);

		std::vector<std::shared_ptr<fpga::ip::AuroraXilinx>> aurora_channels;
		for (int i = 0; i < 4; i++) {
			auto name = fmt::format("aurora_8b10b_ch{}", i);
			auto id = fpga::ip::IpIdentifier("xilinx.com:ip:aurora_8b10b:", name);
			auto aurora = std::dynamic_pointer_cast<fpga::ip::AuroraXilinx>(handle->card->lookupIp(id));
			if (aurora == nullptr) {
				logger->error("No Aurora interface found on FPGA");
				return nullptr;
			}

			aurora_channels.push_back(aurora);
		}

		handle->dma = std::dynamic_pointer_cast<fpga::ip::Dma>
					(handle->card->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_dma:")));
		if (handle->dma == nullptr) {
			logger->error("No DMA found on FPGA ");
			return nullptr;
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
		parsedConnectString.configCrossBar(handle->dma, aurora_channels);

		return handle;
	} catch (const RuntimeError &e) {
		logger->error("Error: {}", e.what());
		return nullptr;
	} catch (const std::exception &e) {
		logger->error("Error: {}", e.what());
		return nullptr;
	} catch (...) {
		logger->error("Unknown error");
		return nullptr;
	}
}

void villasfpga_destroy(villasfpga_handle handle)
{
	delete handle;
}

int villasfpga_alloc(villasfpga_handle handle, villasfpga_memory *mem, size_t size)
{
	try {
		auto &alloc = villas::HostRam::getAllocator();
		*mem = new villasfpga_memory_t;
		(*mem)->block = alloc.allocateBlock(size);
		return villasfpga_register(handle, mem);
	} catch (const RuntimeError &e) {
		logger->error("Failed to allocate memory: {}", e.what());
		return -1;
	}
}
int villasfpga_register(villasfpga_handle handle, villasfpga_memory *mem)
{
	try {
		handle->dma->makeAccesibleFromVA((*mem)->block);
		return 0;
	} catch (const RuntimeError &e) {
		logger->error("Failed to register memory: {}", e.what());
		return -1;
	}
}
int villasfpga_free(villasfpga_memory mem)
{
	try {
		delete mem;
		return 0;
	} catch (const RuntimeError &e) {
		logger->error("Failed to free memory: {}", e.what());
		return -1;
	}
}

int villasfpga_read(villasfpga_handle handle, villasfpga_memory mem, size_t size)
{
	try {
		if (!handle->dma->read(*mem->block, size)) {
			logger->error("Failed to read from device");
			return -1;
		}
		return 0;
	} catch (const RuntimeError &e) {
		logger->error("Failed to read memory: {}", e.what());
		return -1;
	}
}

int villasfpga_read_complete(villasfpga_handle handle, size_t *size)
{
	try {
		auto readComp = handle->dma->readComplete();
		logger->debug("Read {} bytes", readComp.bytes);
		*size = readComp.bytes;
		return 0;
	} catch (const RuntimeError &e) {
		logger->error("Failed to read memory: {}", e.what());
		return -1;
	}
}

int villasfpga_write(villasfpga_handle handle, villasfpga_memory mem, size_t size)
{
	try {
		if (!handle->dma->write(*mem->block, size)) {
			logger->error("Failed to write to device");
			return -1;
		}
		return 0;
	} catch (const RuntimeError &e) {
		logger->error("Failed to write memory: {}", e.what());
		return -1;
	}
}

int villasfpga_write_complete(villasfpga_handle handle, size_t *size)
{
	try {
		auto writeComp = handle->dma->writeComplete();
		logger->debug("Wrote {} bytes", writeComp.bytes);
		*size = writeComp.bytes;
		return 0;
	} catch (const RuntimeError &e) {
		logger->error("Failed to write memory: {}", e.what());
		return -1;
	}
}

void* villasfpga_get_ptr(villasfpga_memory mem)
{
	return (void*)MemoryManager::get().getTranslationFromProcess(mem->block->getAddrSpaceId()).getLocalAddr(0);
}

