/* I2C driver
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fmt/ostream.h>
#include <villas/config.hpp>
#include <villas/exceptions.hpp>
#include <villas/fpga/node.hpp>
#include <villas/memory.hpp>
#include <xilinx/xiic.h>

namespace villas {
namespace fpga {
namespace ip {

class I2c : public Node {
public:
  friend class I2cFactory;

  I2c();
  virtual ~I2c();
  virtual bool init() override;
  bool reset() override;
  bool write(u8 address, std::vector<u8> &data);
  bool read(u8 address, std::vector<u8> &data, size_t max_read);

  int transmitIntrs;
  int receiveIntrs;
  int statusIntrs;

private:
  static constexpr char registerMemory[] = "Reg";
  static constexpr char i2cInterrupt[] = "iic2intc_irpt";

  XIic xIic;
  XIic_Config xConfig;

  std::mutex hwLock;

  bool configDone;
  bool polling;
};
  class I2cFactory : NodeFactory {

  public:
    virtual std::string getName() const { return "i2c"; }

    virtual std::string getDescription() const {
      return "Xilinx's AXI4 iic IP";
    }

  private:
    virtual Vlnv getCompatibleVlnv() const {
      return Vlnv("xilinx.com:ip:axi_iic:");
    }

    // Create a concrete IP instance
    Core *make() const { return new I2c; };

  protected:
    virtual void parse(Core &ip, json_t *json) override;
    virtual void configurePollingMode(Core &ip, PollingMode mode) override {
      dynamic_cast<I2c &>(ip).polling = (mode == POLL);
    }
  };
} // namespace ip
} // namespace fpga
} // namespace villas

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::fpga::ip::I2c> : public fmt::ostream_formatter {};
#endif
