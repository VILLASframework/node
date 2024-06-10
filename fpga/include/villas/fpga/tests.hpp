/* Fpga unit tests
 * SPDX-FileCopyrightText: 2024 Pascal Henry Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <spdlog/common.h>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/utils.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace fpga;

class Tests {
private:
  inline static auto logger = villas::logging.get("Fpga Tests");

public:
  static void writeToDmaFromStdIn(std::shared_ptr<fpga::ip::Dma> dma) {
    auto &alloc = villas::HostRam::getAllocator();
    const std::shared_ptr<villas::MemoryBlock> block =
        alloc.allocateBlock(0x200 * sizeof(float));
    villas::MemoryAccessor<float> mem = *block;
    dma->makeAccesibleFromVA(block);

    logger->info(
        "Please enter values to write to the device, separated by ';'");

    while (true) {
      // Read values from stdin
      std::string line;
      std::getline(std::cin, line);
      auto values = villas::utils::tokenize(line, ";");

      size_t i = 0;
      for (auto &value : values) {
        if (value.empty())
          continue;

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
  }

  static void readFromDmaToStdOut(
      std::shared_ptr<villas::fpga::ip::Dma> dma,
      std::unique_ptr<villas::fpga::BufferedSampleFormatter> formatter) {
    auto &alloc = villas::HostRam::getAllocator();

    const std::shared_ptr<villas::MemoryBlock> block =
        alloc.allocateBlock(0x200 * sizeof(uint32_t));
    villas::MemoryAccessor<int32_t> mem = *block;

    dma->makeAccesibleFromVA(block);

    std::ios::sync_with_stdio(false);

    // Setup read transfer
    //dma->read(*block[0], block[0]->getSize());

    while (true) {
      logger->trace("Read from stream and write to address {}:{:#x}",
                    block->getAddrSpaceId(), block->getOffset());
      // We could use the number of interrupts to determine if we missed a chunk of data
      dma->read(*block, block->getSize());
      auto c = dma->readComplete();

      if (c.interrupts > 1) {
        logger->warn("Missed {} interrupts", c.interrupts - 1);
      }

      logger->debug("bytes: {}, intrs: {}, bds: {}", c.bytes, c.interrupts,
                    c.bds);
      try {
        for (size_t i = 0; i * 4 < c.bytes; i++) {
          int32_t ival = mem[i];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
          float fval =
              *((float *)(&ival)); // cppcheck-suppress invalidPointerCast
#pragma GCC diagnostic pop
          formatter->format(fval);
          printf("%#x\n", ival);
        }
        formatter->output(std::cout);
      } catch (const std::exception &e) {
        logger->warn("Failed to output data: {}", e.what());
      }
    }
  }

  static void writeRead(std::shared_ptr<Card> card) {
    auto dma = std::dynamic_pointer_cast<fpga::ip::Dma>(
        card->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_dma:")));

    auto &alloc = HostRam::getAllocator();

    std::shared_ptr<villas::MemoryBlock> blockRx =
        alloc.allocateBlock(0x200 * sizeof(float));
    std::shared_ptr<villas::MemoryBlock> blockTx =
        alloc.allocateBlock(0x200 * sizeof(float));
    villas::MemoryAccessor<float> memRx = *blockRx;
    villas::MemoryAccessor<float> memTx = *blockTx;

    dma->makeAccesibleFromVA(blockRx);
    dma->makeAccesibleFromVA(blockTx);

    dma->read(*blockRx, sizeof(float));

    memTx[0] = 1337;
    // Initiate write transfer
    bool state = dma->write(*blockTx, sizeof(float));
    if (!state)
      logger->error("Failed to write to device");

    auto writeComp = dma->writeComplete();
    logger->debug("Wrote {} bytes", writeComp.bytes);

    auto c = dma->readComplete();

    if (c.interrupts > 1) {
      logger->warn("Missed {} interrupts", c.interrupts - 1);
    }

    logger->debug("bytes: {}, intrs: {}, bds: {}", c.bytes, c.interrupts,
                  c.bds);

    logger->info("Read: {}", memRx[0]);
  }
};