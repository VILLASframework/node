/* GPU unit tests.
 *
* Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <criterion/criterion.h>

#include <map>
#include <string>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/bram.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/log.hpp>

#include <villas/utils.hpp>

#include "global.hpp"

#include <villas/gpu.hpp>
#include <villas/memory.hpp>

using namespace villas;

// cppcheck-suppress unknownMacro
Test(fpga, gpu_dma, .description = "GPU DMA tests") {
  auto logger = Log::get("unit-test:dma");

  auto &card = state.cards.front();

  auto gpuPlugin = Plugin::Registry<GpuFactory>("cuda");
  cr_assert_not_null(gpuPlugin, "No GPU plugin found");

  auto gpus = gpuPlugin->make();
  cr_assert(gpus.size() > 0, "No GPUs found");

  // Just get first cpu
  auto &gpu = gpus.front();

  size_t count = 0;
  for (auto &ip : card->ips) {
    // Skip non-dma IPs
    if (*ip != fpga::Vlnv("xilinx.com:ip:axi_bram_ctrl:"))
      continue;

    logger->info("Testing {}", *ip);

    auto bram = dynamic_cast<fpga::ip::Bram *>(ip.get());
    cr_assert_not_null(bram, "Couldn't find BRAM");

    count++;

    size_t len = 4 * (1 << 10);

    // Allocate memory to use with DMA

    auto bram0 = bram->getAllocator().allocate<char>(len);
    auto bram1 = bram->getAllocator().allocate<char>(len);

    gpu->makeAccessibleFromPCIeOrHostRam(bram0.getMemoryBlock());
    gpu->makeAccessibleFromPCIeOrHostRam(bram1.getMemoryBlock());

    auto hostRam0 = HostRam::getAllocator().allocate<char>(len);
    auto hostRam1 = HostRam::getAllocator().allocate<char>(len);

    gpu->makeAccessibleFromPCIeOrHostRam(hostRam0.getMemoryBlock());
    gpu->makeAccessibleFromPCIeOrHostRam(hostRam1.getMemoryBlock());

    auto dmaRam0 = HostDmaRam::getAllocator().allocate<char>(len);
    auto dmaRam1 = HostDmaRam::getAllocator().allocate<char>(len);

    gpu->makeAccessibleFromPCIeOrHostRam(dmaRam0.getMemoryBlock());
    gpu->makeAccessibleFromPCIeOrHostRam(dmaRam1.getMemoryBlock());

    auto gpuMem0 = gpu->getAllocator().allocate<char>(64 << 10);
    auto gpuMem1 = gpu->getAllocator().allocate<char>(64 << 10);

    gpu->makeAccessibleToPCIeAndVA(gpuMem0.getMemoryBlock());
    gpu->makeAccessibleToPCIeAndVA(gpuMem1.getMemoryBlock());

    //		auto &src = bram0;
    //		auto &dst = bram1;

    //		auto &src = hostRam0;
    //		auto &dst = hostRam1;

    auto &src = dmaRam0;
    //		auto &dst = dmaRam1;

    //		auto &src = gpuMem0;
    auto &dst = gpuMem1;

    std::list<std::pair<std::string, std::function<void()>>> memcpyFuncs = {
        {"cudaMemcpy",
         [&]() {
           gpu->memcpySync(src.getMemoryBlock(), dst.getMemoryBlock(), len);
         }},
        {"CUDA kernel",
         [&]() {
           gpu->memcpyKernel(src.getMemoryBlock(), dst.getMemoryBlock(), len);
         }},
    };

    auto dmaIp = card->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_dma:"));
    auto dma = std::dynamic_pointer_cast<fpga::ip::Dma>(dmaIp);

    if (dma != nullptr and dma->connectLoopback()) {
      memcpyFuncs.push_back({"DMA memcpy", [&]() {
                               dma->makeAccesibleFromVA(src.getMemoryBlock());
                               dma->makeAccesibleFromVA(dst.getMemoryBlock());
                               dma->memcpy(src.getMemoryBlock(),
                                           dst.getMemoryBlock(), len);
                             }});
    }

    for (auto &[name, memcpyFunc] : memcpyFuncs) {
      logger->info("Testing {}", name);

      // Get new random data
      const size_t lenRandom = utils::read_random(&src, len);
      cr_assert(len == lenRandom, "Failed to get random data");

      memcpyFunc();
      const bool success = memcmp(&src, &dst, len) == 0;

      logger->info("  {}", success ? CLR_GRN("Passed") : CLR_RED("Failed"));
    }

    MemoryManager::getGraph().dump();
  }

  cr_assert(count > 0, "No BRAM found");
}
