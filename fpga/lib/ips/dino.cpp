/* Driver for wrapper around standard Xilinx Aurora (xilinx.com:ip:aurora_8b10b)
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdint>

#include <villas/utils.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/dino.hpp>

using namespace villas::fpga::ip;

Dino::Dino() : Node(), i2cdev(nullptr), i2c_channel(0), configDone(false) {}

Dino::~Dino() {}

bool Dino::init() {
  if (!configDone) {
    logger->error("Dino configuration not done yet");
    throw RuntimeError("Dino configuration not done yet");
  }
  if (i2cdev == nullptr) {
    i2cdev = std::dynamic_pointer_cast<fpga::ip::I2c>(
        card->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_iic:")));
    if (i2cdev == nullptr) {
      logger->error("No I2C found on FPGA");
      throw RuntimeError(
          "Dino requires and I2C device but none was found on FPGA");
    } else {
      logger->debug("Found I2C on FPGA");
    }
  }
  configureHardware();
  return true;
}

void Dino::setIoextDir(IoextPorts ports) {
  if (i2cdev == nullptr) {
    throw RuntimeError("I2C device not set");
  }
  std::vector<u8> data = {I2C_IOEXT_REG_DIR, ports.raw};
  i2cdev->write(I2C_IOEXT_ADDR, data);
}

void Dino::setIoextOut(IoextPorts ports) {
  if (i2cdev == nullptr) {
    throw RuntimeError("I2C device not set");
  }
  std::vector<u8> data = {I2C_IOEXT_REG_OUT, ports.raw};
  i2cdev->write(I2C_IOEXT_ADDR, data);
}

Dino::IoextPorts Dino::getIoextDir() {
  if (i2cdev == nullptr) {
    throw RuntimeError("I2C device not set");
  }
  std::vector<u8> data;
  i2cdev->readRegister(I2C_IOEXT_ADDR, I2C_IOEXT_REG_DIR, data, 1);
  IoextPorts ports;
  ports.raw = data[0];
  return ports;
}

Dino::IoextPorts Dino::getIoextDirectionRegister() {
  if (i2cdev == nullptr) {
    throw RuntimeError("I2C device not set");
  }
  i2cdev->getSwitch().setAndLockChannel(i2c_channel);
  auto ret = getIoextDir();
  i2cdev->getSwitch().unlockChannel();
  return ret;
}

Dino::IoextPorts Dino::getIoextOut() {
  if (i2cdev == nullptr) {
    throw RuntimeError("I2C device not set");
  }
  std::vector<u8> data;
  i2cdev->readRegister(I2C_IOEXT_ADDR, I2C_IOEXT_REG_OUT, data, 1);
  IoextPorts ports;
  ports.raw = data[0];
  return ports;
}

Dino::IoextPorts Dino::getIoextOutputRegister() {
  if (i2cdev == nullptr) {
    throw RuntimeError("I2C device not set");
  }
  i2cdev->getSwitch().setAndLockChannel(i2c_channel);
  auto ret = getIoextOut();
  i2cdev->getSwitch().unlockChannel();
  return ret;
}

DinoAdc::DinoAdc() : Dino() {}

DinoAdc::~DinoAdc() {}

void DinoAdc::configureHardware() {
  if (!configDone) {
    logger->error("ADC configuration not done yet");
    throw RuntimeError("ADC configuration not done yet");
  }
  if (i2cdev == nullptr) {
    throw RuntimeError("I2C device not set");
  }
  i2cdev->getSwitch().setAndLockChannel(i2c_channel);
  IoextPorts ioext = {.raw = 0};
  ioext.fields.sat_detect = true;
  setIoextDir(ioext);
  auto readback = getIoextDir();
  if (readback.raw != ioext.raw) {
    logger->error("Ioext direction register readback incorrect: {:#x} != {:#x}",
                  readback.raw, ioext.raw);
    throw RuntimeError("Failed to set IOEXT direction register");
  }
  logger->debug("ADC Ioext: Direction register configured to {}", readback);
  ioext.raw = 0;
  ioext.fields.data_dir = true;
  ioext.fields.status_led = true;
  ioext.fields.n_we = true;
  ioext.fields.input_zero = true;
  setIoextOut(ioext);
  ioext.fields.n_we = false;
  setIoextOut(ioext);
  readback = getIoextOut();
  if (readback.raw != ioext.raw) {
    logger->error("Ioext output register readback incorrect: {:#x} != {:#x}",
                  readback.raw, ioext.raw);
    throw RuntimeError("Failed to set IOEXT output register");
  }
  i2cdev->getSwitch().unlockChannel();
  logger->debug("ADC Ioext: Output register configured to {}", readback);
}

void DinoAdc::setRegisterConfig(std::shared_ptr<Register> reg,
                                double sampleRate) {
  // This is Dino specific for now - we should possibly move this to Dino in the future
  constexpr double dinoClk = 25e6; // Dino is clocked with 25 Mhz

  uint32_t dinoTimerVal = static_cast<uint32_t>(dinoClk / sampleRate);
  double rateError = dinoClk / dinoTimerVal - sampleRate;
  reg->setRegister(
      0,
      dinoTimerVal); // Timer value for generating ADC trigger signal
  reg->setRegister(1, -0.001615254F); // Scale factor for ADC value
  reg->setRegister(2, 10.8061F);      // Offset for ADC value
  uint32_t rate = reg->getRegister(0);
  float scale = reg->getRegisterFloat(1);
  float offset = reg->getRegisterFloat(2);
  logging.get("Dino")->info("Check: Register configuration: Rate: {}, Scale: "
                            "{}, Offset: {}, Rate-Error: {} Hz",
                            rate, scale, offset, rateError);
}

DinoDac::DinoDac() : Dino() {}

DinoDac::~DinoDac() {}

void DinoDac::configureHardware() {
  if (!configDone) {
    logger->error("DAC configuration not done yet");
    throw RuntimeError("DAC configuration not done yet");
  }
  if (i2cdev == nullptr) {
    throw RuntimeError("I2C device not set");
  }
  i2cdev->getSwitch().setAndLockChannel(i2c_channel);
  IoextPorts ioext = {.raw = 0};
  setIoextDir(ioext);
  auto readback = getIoextDir();
  if (readback.raw != ioext.raw) {
    logger->error("Ioext direction register readback incorrect: {:#x} != {:#x}",
                  readback.raw, ioext.raw);
    throw RuntimeError("Failed to set IOEXT direction register");
  }
  logger->debug("DAC Ioext: Direction register configured to {}", readback);
  ioext.fields.status_led = true;
  // Default gain is 1. Although not really necessary, let's be explicit here
  ioext.fields.gain_lsb = Gain::GAIN_1 & 0x1;
  ioext.fields.gain_msb = Gain::GAIN_1 & 0x2;
  setIoextOut(ioext);
  readback = getIoextOut();
  if (readback.raw != ioext.raw) {
    logger->error("Ioext output register readback incorrect: {:#x} != {:#x}",
                  readback.raw, ioext.raw);
    throw RuntimeError("Failed to set IOEXT output register");
  }
  i2cdev->getSwitch().unlockChannel();
  logger->debug("DAC Ioext: Output register configured to {}", readback);
}

void DinoDac::setGain(Gain gain) {
  if (i2cdev == nullptr) {
    throw RuntimeError("I2C device not set");
  }
  i2cdev->getSwitch().setAndLockChannel(i2c_channel);
  IoextPorts ioext = getIoextOut();
  ioext.fields.gain_lsb = gain & 0x1;
  ioext.fields.gain_msb = gain & 0x2;
  setIoextOut(ioext);
  i2cdev->getSwitch().unlockChannel();
}

Dino::Gain DinoDac::getGain() {
  if (i2cdev == nullptr) {
    throw RuntimeError("I2C device not set");
  }
  i2cdev->getSwitch().setAndLockChannel(i2c_channel);
  IoextPorts ioext = getIoextOut();
  auto ret =
      static_cast<Gain>((ioext.fields.gain_msb << 1) | ioext.fields.gain_lsb);
  i2cdev->getSwitch().unlockChannel();
  return ret;
}

void DinoFactory::parse(Core &ip, json_t *cfg) {
  NodeFactory::parse(ip, cfg);

  auto &dino = dynamic_cast<Dino &>(ip);
  json_error_t err;
  int i2c_channel;
  int ret =
      json_unpack_ex(cfg, &err, 0, "{ s: i }", "i2c_channel", &i2c_channel);
  if (ret != 0) {
    throw ConfigError(cfg, err, "", "Failed to parse Dino configuration");
  }
  if (i2c_channel < 0 || i2c_channel >= 8) {
    throw ConfigError(cfg, err, "", "Invalid I2C channel");
  }
  dino.i2c_channel = static_cast<uint8_t>(i2c_channel);

  dino.configDone = true;
}

static DinoAdcFactory fAdc;
static DinoDacFactory fDac;
