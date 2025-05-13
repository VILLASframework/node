/* Driver for wrapper around standard Xilinx Aurora (xilinx.com:ip:aurora_8b10b)
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023-2024 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdint>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/dino.hpp>
#include <villas/utils.hpp>

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
  constexpr double dinoClk = 25e6; // Dino is clocked with 25 Mhz
  // From the data sheets we can assume an analog delay of 828e-9s
  // However this will eat into our computation time, so it should be
  // configurable. Let's assume 0 until we implement this.
  constexpr double dinoDacDelay = 0; // Delay for DAC to settle
  constexpr size_t dinoRegisterTimer = 0;
  constexpr size_t dinoRegisterAdcScale = 1;
  constexpr size_t dinoRegisterAdcOffset = 2;
  constexpr size_t dinoRegisterDacExternalTrig = 4;
  constexpr size_t dinoRegisterStsActive = 5;
  constexpr size_t dinoRegisterDacScale = 6;
  constexpr size_t dinoRegisterDacOffset = 7;
  constexpr size_t dinoRegisterTimerPreThresh = 8;

  // -1 because the timer counts from 0 to the value set in the register. Should really be fixed in hardware.
  uint32_t dinoTimerVal = static_cast<uint32_t>(dinoClk / sampleRate) - 1;
  uint32_t dinoDacDelayCycles = static_cast<uint32_t>(dinoClk * dinoDacDelay);
  double rateError = dinoClk / (dinoTimerVal + 1) - sampleRate;

  // Timer value for generating ADC trigger signal
  reg->setRegister(dinoRegisterTimer, dinoTimerVal);

  // The following are calibration values for the ADC and DAC. Scale
  // sets an factor to be multiplied with the input value. This is the
  // raw 16 bit ADC value for the ADC and the float value from VILLAS for
  // the DAC. Offset is a value to be added to the result of the multiplication.
  // All values are IEE 754 single precision floating point values.
  // Calibration for ADC filter with C=330pF and R=2,2kOhm
  // TODO: These values should be read from the FPGA or configured via the configuration file.
  reg->setRegister(dinoRegisterAdcScale,
                   0.0016874999385349976F); // Scale factor for ADC value
  reg->setRegister(dinoRegisterAdcOffset,
                   -11.365293957141239F); // Offset for ADC value
  reg->setRegister(dinoRegisterDacScale,
                   3204.7355379027363F); // Scale factor for DAC value
  reg->setRegister(dinoRegisterDacOffset,
                   32772.159015058445F); // Offset for DAC value
  reg->setRegister(dinoRegisterDacExternalTrig,
                   (uint32_t)0x0); // External trigger for DAC

  if (dinoTimerVal > dinoDacDelayCycles) {
    reg->setRegister(dinoRegisterTimerPreThresh,
                     dinoTimerVal - dinoDacDelayCycles);
  } else {
    reg->setRegister(dinoRegisterTimerPreThresh, dinoTimerVal);
  }

  uint32_t rate = reg->getRegister(dinoRegisterTimer);
  float adcScale = reg->getRegisterFloat(dinoRegisterAdcScale);
  float adcOffset = reg->getRegisterFloat(dinoRegisterAdcOffset);
  float dacScale = reg->getRegisterFloat(dinoRegisterDacScale);
  float dacOffset = reg->getRegisterFloat(dinoRegisterDacOffset);
  uint32_t dacExternalTrig = reg->getRegister(dinoRegisterDacExternalTrig);
  uint32_t stsActive = reg->getRegister(dinoRegisterStsActive);
  uint32_t ratePreThresh = reg->getRegister(dinoRegisterTimerPreThresh);
  Log::get("Dino")->info(
      "Check: Register configuration: TimerThresh: {}, Rate-Error: {} Hz, ADC "
      "Scale: {}, ADC Offset: {}, DAC Scale: {}, DAC Offset: {}, DAC External "
      "Trig: {:#x}, STS Active: {:#x}, TimerPreThresh: {}",
      rate, rateError, adcScale, adcOffset, dacScale, dacOffset,
      dacExternalTrig, stsActive, ratePreThresh);
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
