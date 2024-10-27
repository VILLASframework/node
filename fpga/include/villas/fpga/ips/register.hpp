/* Driver for register interface 'registerif'
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

class Register : public Node {
public:
  Register();
  virtual ~Register();
  virtual bool init() override;
  virtual bool check() override;
  void setRegister(size_t reg, uint32_t value);
  void setRegister(size_t reg, float value);
  uint32_t getRegister(size_t reg);
  float getRegisterFloat(size_t reg);
  void resetRegister(size_t reg);
  void resetAllRegisters();

protected:
  const size_t registerNum = 9;
  const size_t registerSize = 32;
  static constexpr char registerMemory[] = "reg0";
  std::list<MemoryBlockName> getMemoryBlocks() const override {
    return {registerMemory};
  }
};

} // namespace ip
} // namespace fpga
} // namespace villas

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::fpga::ip::Register>
    : public fmt::ostream_formatter {};
#endif
