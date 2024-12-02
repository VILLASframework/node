/* Driver for AXI Stream read cache.
 *
 * This module is used to lower latency of
 * a DMA Scatter Gather engine's descriptor fetching. The driver allows for
 * invalidating the cache.
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2024 Niklas Eiling
 * SPDX-License-Identifier: Apache-2.0
 */

#include <xilinx/xil_io.h>

#include <villas/fpga/ips/axis_cache.hpp>

using namespace villas::fpga::ip;

#define REGISTER_OUT(NUM) (4 * NUM)

AxisCache::AxisCache() : Node() {}

bool AxisCache::init() {
  invalidate();
  return true;
}

bool AxisCache::check() {

  logger->debug("Checking register interface: Base address: 0x{:08x}",
                getBaseAddr(registerMemory));
  uint32_t buf;

  // We should not change the rate register, because this can lead to hardware fault, so start at 1
  for (size_t i = 1; i < registerNum; i++) {
    setRegister(i, static_cast<uint32_t>(0x00FF00FF));
  }

  for (size_t i = 1; i < registerNum; i++) {
    if (!getRegister(i, buf)) {
      logger->error("Failed to read register {}", i);
      return false;
    }
    if (buf != 0x00FF00FF) {
      logger->error("Register {}: 0x{:08x} != 0x{:08x}", i, buf, i);
      return false;
    }
  }

  resetAllRegisters();

  for (size_t i = 0; i < registerNum; i++) {
    if (!getRegister(i, buf)) {
      logger->error("Failed to read register {}", i);
      return false;
    }
    logger->debug("Register {}: 0x{:08x}", i, buf);
  }

  return true;
}

void AxisCache::invalidate() {
  setRegister(0, 1U << 31);
  logger->info("invalidated AXIS cache.");
}

bool AxisCache::setRegister(size_t reg, uint32_t value) {
  if (reg >= registerNum) {
    logger->error("Register index out of range: {}/{}", reg, registerNum);
    return false;
  }
  Xil_Out32(getBaseAddr(registerMemory) + REGISTER_OUT(reg), value);
  return true;
}

bool AxisCache::getRegister(size_t reg, uint32_t &value) {
  if (reg >= registerNum) {
    logger->error("Register index out of range: {}/{}", reg, registerNum);
    return false;
  }
  value = Xil_In32(getBaseAddr(registerMemory) + REGISTER_OUT(reg));
  return true;
}

bool AxisCache::resetRegister(size_t reg) { return setRegister(reg, 0); }

bool AxisCache::resetAllRegisters() {
  bool result = true;
  for (size_t i = 1; i < registerNum; i++) {
    result &= resetRegister(i);
  }
  return result;
}

AxisCache::~AxisCache() {}

static char n[] = "axis_cache";
static char d[] = "Register interface VHDL module 'axi_read_cache'";
static char v[] = "xilinx.com:module_ref:axi_read_cache:";
static CorePlugin<AxisCache, n, d, v> f;
