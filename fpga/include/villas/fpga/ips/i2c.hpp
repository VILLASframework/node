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

  virtual ~I2c();
  virtual bool init() override;
  bool reset() override;
  bool write(std::list<u8> &data);
  bool read(std::list<u8> &data, size_t max_read);

private:
  static constexpr char registerMemory[] = "Reg";

  XIic xIic;
  XIic_Config xConfig;

  std::mutex hwLock;

  bool configDone = false;

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
    Core *make() const { return new Dma; };

  protected:
    virtual void parse(Core &ip, json_t *json) override;

    virtual void configurePollingMode(Core &ip, PollingMode mode) override {
      dynamic_cast<Dma &>(ip).polling = (mode == POLL);
    }
  };

} // namespace ip
} // namespace fpga
} // namespace villas

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::fpga::ip::I2c> : public fmt::ostream_formatter {};
#endif
