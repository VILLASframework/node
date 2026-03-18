/* Driver for AXI Stream read cache. This module is used to lower latency of
 * a DMA Scatter Gather engine's descriptor fetching. The driver allows for
 * invalidating the cache.
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2024 Niklas Eiling
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class AxisCache : public Node {
public:
  AxisCache();
  ~AxisCache() override;
  bool init() override;
  bool check() override;
  void invalidate();

protected:
  const size_t registerNum = 1;
  const size_t registerSize = 32;
  static constexpr char registerMemory[] = "reg0";
  std::list<MemoryBlockName> getMemoryBlocks() const override {
    return {registerMemory};
  }
  bool setRegister(size_t reg, uint32_t value);
  bool getRegister(size_t reg, uint32_t &value);
  bool resetRegister(size_t reg);
  bool resetAllRegisters();
};

} // namespace ip
} // namespace fpga
} // namespace villas

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::fpga::ip::AxisCache>
    : public fmt::ostream_formatter {};
#endif
