/* I2C driver
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sstream>
#include <string>

#include <xilinx/xiic.h>

#include <villas/fpga/ips/i2c.hpp>
#include <villas/fpga/ips/intc.hpp>
#include <villas/log.hpp>

using namespace villas::fpga::ip;

I2c::I2c()
    : Node(), transmitIntrs(0), receiveIntrs(0), statusIntrs(0), xIic(),
      xConfig(), hwLock(), configDone(false), initDone(false), polling(false),
      switchInstance(nullptr) {}

I2c::~I2c() { I2c::reset(); }

static void SendHandler(I2c *i2c, __attribute__((unused)) int bytesSend) {
  i2c->transmitIntrs++;
}
static void ReceiveHandler(I2c *i2c, __attribute__((unused)) int bytesSend) {
  i2c->receiveIntrs++;
}
static void StatusHandler(I2c *i2c, __attribute__((unused)) int event) {
  i2c->statusIntrs++;
}

bool I2c::init() {
  int ret;
  if (!configDone) {
    throw RuntimeError("I2C configuration not done");
  }
  if (initDone) {
    logger->warn("I2C already initialized");
    return true;
  }
  xConfig.BaseAddress = getBaseAddr(registerMemory);
  hwLock.lock();
  ret = XIic_CfgInitialize(&xIic, &xConfig, xConfig.BaseAddress);
  if (ret != XST_SUCCESS) {
    throw RuntimeError("Failed to initialize I2C");
  }

  XIic_SetSendHandler(&xIic, this, (XIic_Handler)SendHandler);
  XIic_SetRecvHandler(&xIic, this, (XIic_Handler)ReceiveHandler);
  XIic_SetStatusHandler(&xIic, this, (XIic_StatusHandler)StatusHandler);

  irqs[i2cInterrupt].irqController->enableInterrupt(irqs[i2cInterrupt],
                                                    polling);
  hwLock.unlock();
  initDone = true;
  return true;
}

bool I2c::check() {
  if (!initDone) {
    throw RuntimeError("I2C not initialized");
  }
  return getSwitch().selfTest();
}

bool I2c::reset() {
  // we cannot lock here because this may be called in a destructor
  XIic_Reset(&xIic);
  irqs[i2cInterrupt].irqController->disableInterrupt(irqs[i2cInterrupt]);
  initDone = false;
  return true;
}

void I2c::driverWriteBlocking(u8 *dataPtr, size_t size) {
  int ret;
  int intrRetries = 1;
  transmitIntrs = 0;
  xIic.Stats.TxErrors = 0;
  do {
    ret = XIic_MasterSend(&xIic, dataPtr, size);
    if (ret == XST_IIC_BUS_BUSY) {
      waitForBusNotBusy();
    } else if (ret != XST_SUCCESS) {
      throw RuntimeError("Failed to send I2C data. code: {}", ret);
    }
  } while (ret == XST_IIC_BUS_BUSY && --intrRetries >= 0);
  intrRetries = 10;
  while (transmitIntrs == 0 && intrRetries > 0) {
    irqs[i2cInterrupt].irqController->waitForInterrupt(irqs[i2cInterrupt].num);
    XIic_InterruptHandler(&xIic);
    --intrRetries;
  }
  if (intrRetries == 0) {
    throw RuntimeError(
        "Failed to send I2C data: No transmit interrupt after 10 tries.");
  }
  if (xIic.Stats.TxErrors > 0) {
    throw RuntimeError("Failed to send I2C data: {} TX errors",
                       xIic.Stats.TxErrors);
  }
}

void I2c::driverReadBlocking(u8 *dataPtr, size_t max_read) {
  int ret;
  int intrRetries = 1;
  receiveIntrs = 0;
  do {
    ret = XIic_MasterRecv(&xIic, dataPtr, max_read);
    if (ret == XST_IIC_BUS_BUSY) {
      waitForBusNotBusy();
    } else if (ret != XST_SUCCESS) {
      throw RuntimeError("Failed to receive I2C data: code {}", ret);
    }
  } while (ret == XST_IIC_BUS_BUSY && --intrRetries >= 0);
  intrRetries = 10;
  while (receiveIntrs == 0 && intrRetries > 0) {
    irqs[i2cInterrupt].irqController->waitForInterrupt(irqs[i2cInterrupt].num);
    XIic_InterruptHandler(&xIic);
    --intrRetries;
  }
  if (intrRetries == 0) {
    throw RuntimeError(
        "Failed to receive I2C data: No receive interrupt after 10 tries.");
  }
}

void I2c::waitForBusNotBusy() {
  int retries = 10;
  uint32_t irqStatus;
  do {
    irqs[i2cInterrupt].irqController->waitForInterrupt(irqs[i2cInterrupt].num);
    irqStatus =
        XIic_ReadIisr(xIic.BaseAddress) & XIic_ReadIier(xIic.BaseAddress);
  } while (!(irqStatus & XIIC_INTR_BNB_MASK) && --retries > 0);
  //Deactivate BusNotBusy interrupt
  XIic_WriteIier(xIic.BaseAddress,
                 XIic_ReadIier(xIic.BaseAddress) & ~(XIIC_INTR_BNB_MASK));
  if (retries == 0) {
    throw RuntimeError("I2C bus stayed busy after 10 interrupts");
  }
}

bool I2c::write(u8 address, std::vector<u8> &data) {
  int ret;

  if (!initDone) {
    throw RuntimeError("I2C not initialized");
  }

  hwLock.lock();
  ret =
      XIic_SetAddress(&xIic, XII_ADDR_TO_SEND_TYPE, static_cast<int>(address));
  if (ret != XST_SUCCESS) {
    throw RuntimeError("Failed to set I2C address");
  }

  ret = XIic_Start(&xIic);
  if (ret != XST_SUCCESS) {
    throw RuntimeError("Failed to start I2C");
  }

  driverWriteBlocking(data.data(), data.size());

  ret = XIic_Stop(&xIic);
  if (ret != XST_SUCCESS) {
    throw RuntimeError("Failed to stop I2C");
  }
  hwLock.unlock();
  return true;
}

bool I2c::read(u8 address, std::vector<u8> &data, size_t max_read) {
  int ret;

  if (!initDone) {
    throw RuntimeError("I2C not initialized");
  }

  data.resize(data.size() + max_read);
  u8 *dataPtr = data.data() + data.size() - max_read;

  hwLock.lock();
  ret =
      XIic_SetAddress(&xIic, XII_ADDR_TO_SEND_TYPE, static_cast<int>(address));
  if (ret != XST_SUCCESS) {
    throw RuntimeError("Failed to set I2C address");
  }

  ret = XIic_Start(&xIic);
  if (ret != XST_SUCCESS) {
    throw RuntimeError("Failed to start I2C");
  }

  driverReadBlocking(dataPtr, max_read);

  ret = XIic_Stop(&xIic);
  if (ret != XST_SUCCESS) {
    throw RuntimeError("Failed to stop I2C");
  }
  hwLock.unlock();

  return XST_SUCCESS;
}

bool I2c::readRegister(u8 address, u8 reg, std::vector<u8> &data,
                       size_t max_read) {

  std::vector<u8> regData = {reg};
  bool ret;
  ret = write(address, regData);
  ret &= read(address, data, max_read);
  return ret;
}

static const uint8_t CHANNEL_MAP[] = I2C_SWITCH_CHANNEL_MAP;
void I2c::Switch::setChannel(uint8_t channel) {
  if (channel >= sizeof(CHANNEL_MAP) / sizeof(CHANNEL_MAP[0])) {
    throw RuntimeError("Invalid channel number {}", channel);
  }
  this->channel = channel;
  readOnce = true;
  std::vector<u8> data = {CHANNEL_MAP[channel]};
  i2c->write(address, data);
}

uint8_t I2c::Switch::getChannel() {
  std::vector<u8> data;
  int retries = 10;
  do {
    i2c->read(address, data, 1);
  } while (readOnce && data[0] != CHANNEL_MAP[channel] && --retries >= 0);
  if (retries == 0) {
    throw RuntimeError(
        "Invalid channel readback after 10 retries: {:x} != {:x}", data[0],
        CHANNEL_MAP[channel]);
  }
  if (!readOnce) {
    for (size_t i = 0; i < sizeof(CHANNEL_MAP) / sizeof(CHANNEL_MAP[0]); ++i) {
      if (data[0] == CHANNEL_MAP[i]) {
        channel = i;
        break;
      }
    }
    readOnce = true;
  }
  return channel;
}

bool I2c::Switch::selfTest() {
  uint8_t readback;
  logger->debug("I2c::Switch self test. Testing {} channels of device {:#x}",
                sizeof(CHANNEL_MAP) / sizeof(CHANNEL_MAP[0]), address);
  for (size_t i = 0; i < sizeof(CHANNEL_MAP) / sizeof(CHANNEL_MAP[0]); ++i) {
    logger->debug("Setting switch to channel {}, data byte {:#x}", i,
                  CHANNEL_MAP[i]);
    setAndLockChannel(i);
    try {
      readback = getChannel();
      logger->debug("Readback channel: {}", readback);
    } catch (const RuntimeError &e) {
      logger->debug("Encoutered error on readback: {}", e.what());
      return false;
    }
    unlockChannel();
  }
  return true;
}

void I2cFactory::parse(Core &ip, json_t *cfg) {
  NodeFactory::parse(ip, cfg);

  auto &i2c = dynamic_cast<I2c &>(ip);

  int i2c_frequency = 0;
  char *component_name = nullptr;

  json_error_t err;
  int ret = json_unpack_ex(cfg, &err, 0, "{ s: { s?: i, s?: i, s?: i, s?: s} }",
                           "parameters", "c_iic_freq", &i2c_frequency,
                           "c_ten_bit_adr", &i2c.xConfig.Has10BitAddr,
                           "c_gpo_width", &i2c.xConfig.GpOutWidth,
                           "component_name", &component_name);
  if (ret != 0) {
    throw ConfigError(cfg, err, "", "Failed to parse DMA configuration for {}",
                      ip.getInstanceName());
  }
  if (component_name != nullptr) {
    char last_letter = component_name[strlen(component_name) - 1];
    if (last_letter >= '0' && last_letter <= '9') {
      i2c.xConfig.DeviceId = last_letter - '0';
    } else {
      throw RuntimeError("Invalid device ID in component name {} for {}",
                         component_name, ip.getInstanceName());
    }
  }

  i2c.configDone = true;
}

static I2cFactory f;
