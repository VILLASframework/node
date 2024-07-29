/* Streaming data from STDIN/OUT to FPGA.
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>
#include <jansson.h>
#include <string>
#include <vector>

#include <CLI11.hpp>
#include <rang.hpp>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/utils.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/core.hpp>
#include <villas/fpga/ips/aurora_xilinx.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/rtds.hpp>
#include <villas/fpga/utils.hpp>
#include <villas/fpga/vlnv.hpp>

using namespace villas;

static std::shared_ptr<kernel::pci::DeviceList> pciDevices;
static auto logger = villas::Log::get("streamer");

int main(int argc, char *argv[]) {
  // Command Line Parser
  CLI::App app{"VILLASfpga data streamer"};

  try {
    std::string configFile;
    app.add_option("-c,--config", configFile, "Configuration file")
        ->check(CLI::ExistingFile);

    std::string fpgaName = "vc707";
    app.add_option("--fpga", fpgaName, "Which FPGA to use");
    app.parse(argc, argv);

    // Logging setup
    spdlog::set_level(spdlog::level::debug);
    fpga::setupColorHandling();

    if (configFile.empty()) {
      logger->error(
          "No configuration file provided/ Please use -c/--config argument");
      return 1;
    }

    auto card = fpga::setupFpgaCard(configFile, fpgaName);

    std::vector<std::shared_ptr<fpga::ip::AuroraXilinx>> aurora_channels;
    for (int i = 0; i < 4; i++) {
      auto name = fmt::format("aurora_8b10b_ch{}", i);
      auto id = fpga::ip::IpIdentifier("xilinx.com:ip:aurora_8b10b:", name);
      auto aurora =
          std::dynamic_pointer_cast<fpga::ip::AuroraXilinx>(card->lookupIp(id));
      if (aurora == nullptr) {
        logger->error("No Aurora interface found on FPGA");
        return 1;
      }

      aurora_channels.push_back(aurora);
    }

    auto dma = std::dynamic_pointer_cast<fpga::ip::Dma>(
        card->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_dma:")));
    if (dma == nullptr) {
      logger->error("No DMA found on FPGA ");
      return 1;
    }

    for (auto aurora : aurora_channels)
      aurora->dump();

      // Configure Crossbar switch
#if 1
    aurora_channels[3]->connect(aurora_channels[3]->getDefaultMasterPort(),
                                dma->getDefaultSlavePort());
    dma->connect(dma->getDefaultMasterPort(),
                 aurora_channels[3]->getDefaultSlavePort());
#else
    dma->connectLoopback();
#endif
    auto &alloc = villas::HostRam::getAllocator();
    const std::shared_ptr<villas::MemoryBlock> block[] = {
        alloc.allocateBlock(0x200 * sizeof(uint32_t)),
        alloc.allocateBlock(0x200 * sizeof(uint32_t))};
    villas::MemoryAccessor<int32_t> mem[] = {*block[0], *block[1]};

    for (auto b : block) {
      dma->makeAccesibleFromVA(b);
    }
    auto &mm = MemoryManager::get();
    mm.getGraph().dump("graph.dot");

    while (true) {
      // Setup read transfer
      dma->read(*block[0], block[0]->getSize());

      // Read values from stdin
      std::string line;
      std::getline(std::cin, line);
      auto values = villas::utils::tokenize(line, ";");

      size_t i = 0;
      for (auto &value : values) {
        if (value.empty())
          continue;

        const int32_t number = std::stoi(value);
        mem[1][i++] = number;
      }

      // Initiate write transfer
      bool state = dma->write(*block[1], i * sizeof(int32_t));
      if (!state)
        logger->error("Failed to write to device");

      auto writeComp = dma->writeComplete();
      logger->info("Wrote {} bytes", writeComp.bytes);

      auto readComp = dma->readComplete();
      auto valuesRead = readComp.bytes / sizeof(int32_t);
      logger->info("Read {} bytes, {} bds, {} interrupts", readComp.bytes,
                   readComp.bds, readComp.interrupts);

      for (size_t i = 0; i < valuesRead; i++)
        std::cerr << mem[0][i] << ";";
      std::cerr << std::endl;
    }
  } catch (const RuntimeError &e) {
    logger->error("Error: {}", e.what());
    return -1;
  } catch (const CLI::ParseError &e) {
    return app.exit(e);
  }

  return 0;
}
