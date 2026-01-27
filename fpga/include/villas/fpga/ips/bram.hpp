/* Block-Raam related helper functions
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2018 Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/fpga/core.hpp>
#include <villas/memory.hpp>

namespace villas {
namespace fpga {
namespace ip {

class BramFactory;

class Bram : public Core {
  friend class BramFactory;

public:
  bool init() override;

  LinearAllocator &getAllocator() { return *allocator; }

private:
  static constexpr const char *memoryBlock = "Mem0";

  std::list<MemoryBlockName> getMemoryBlocks() const override {
    return {memoryBlock};
  }

  size_t size;
  std::unique_ptr<LinearAllocator> allocator;
};

class BramFactory : public CoreFactory {

public:
  std::string getName() const override { return "bram"; }

  std::string getDescription() const override { return "Block RAM"; }

private:
  Vlnv getCompatibleVlnv() const override {
    return Vlnv("xilinx.com:ip:axi_bram_ctrl:");
  }

  // Create a concrete IP instance
  Core *make() const override { return new Bram; };

protected:
  void parse(Core &, json_t *) override;
};

} // namespace ip
} // namespace fpga
} // namespace villas
