/* Driver for wrapper around Dino
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * Author: Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2024 Niklas Eiling
 * SPDX-FileCopyrightText: 2020 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/fpga/ips/i2c.hpp>
#include <villas/fpga/node.hpp>

namespace villas {
namespace fpga {
namespace ip {

class Dino : public Node {
public:
  union IoextPorts {
    struct __attribute__((packed)) {
      bool clk_dir : 1;
      bool data_dir : 1;
      bool status_led : 1;
      bool n_we : 1; // write enable (active low)
      bool input_zero : 1;
      bool sat_detect : 1;
      bool gain_lsb : 1;
      bool gain_msb : 1;
    } fields;
    uint8_t raw;
  };
  enum Gain { GAIN_1 = 0, GAIN_2 = 1, GAIN_5 = 2, GAIN_10 = 3 };

  Dino();
  virtual ~Dino();
  void setI2c(std::shared_ptr<I2c> i2cdev, uint8_t i2c_channel) {
    this->i2cdev = i2cdev;
    this->i2c_channel = i2c_channel;
  }
  virtual void configureHardware() = 0;

  static constexpr const char *masterPort = "M00_AXIS";
  static constexpr const char *slavePort = "S00_AXIS";

  const StreamVertex &getDefaultSlavePort() const {
    return getSlavePort(slavePort);
  }

  const StreamVertex &getDefaultMasterPort() const {
    return getMasterPort(masterPort);
  }

  IoextPorts getIoextDir();
  IoextPorts getIoextOut();

protected:
  std::shared_ptr<I2c> i2cdev;
  uint8_t i2c_channel;

  void setIoextDir(IoextPorts ports);
  void setIoextOut(IoextPorts ports);
};

class DinoAdc : public Dino {
public:
  DinoAdc();
  virtual ~DinoAdc();
  virtual void configureHardware() override;
};

class DinoDac : public Dino {
public:
  DinoDac();
  virtual ~DinoDac();
  virtual void configureHardware() override;
  void setGain(Gain gain);
  Gain getGain();
};

} // namespace ip
} // namespace fpga
} // namespace villas
