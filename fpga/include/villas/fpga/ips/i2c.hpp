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

  class Switch {
  public:
    Switch(I2c *i2c, uint8_t address)
        : i2c(i2c), address(address), channel(0), readOnce(false){};
    Switch(const Switch &other) = delete;
    Switch &operator=(const Switch &other) = delete;
    void setChannel(uint8_t channel);
    uint8_t getChannel();
    void setAddress(uint8_t address) { this->address = address; }
    uint8_t getAddress() { return address; }

  private:
    I2c *i2c;
    uint8_t address;
    uint8_t channel;
    bool readOnce;
  };
  Switch &getSwitch(uint8_t address = 0x70) {
    if (switchInstance == nullptr) {
      switchInstance = std::make_unique<Switch>(this, address);
    } else {
      switchInstance->setAddress(address);
    }
    return *switchInstance;
  }

private:
  static constexpr char registerMemory[] = "Reg";
  static constexpr char i2cInterrupt[] = "iic2intc_irpt";

  XIic xIic;
  XIic_Config xConfig;

  std::mutex hwLock;

  bool configDone;
  bool initDone;
  bool polling;
  std::unique_ptr<Switch> switchInstance;

  std::list<MemoryBlockName> getMemoryBlocks() const {
    return {registerMemory};
  }
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
