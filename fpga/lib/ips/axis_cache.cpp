/* Driver for AXI Stream read cache. This module is used to lower latency of
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
  // we shouldn't change the rate register, because this can lead to hardware fault, so start at 1
  for (size_t i = 1; i < registerNum; i++) {
    setRegister(i, static_cast<uint32_t>(0x00FF00FF));
  }

  for (size_t i = 1; i < registerNum; i++) {
    buf = getRegister(i);
    if (buf != 0x00FF00FF) {
      logger->error("Register {}: 0x{:08x} != 0x{:08x}", i, buf, i);
      return false;
    }
  }

  // Reset Registers
  resetAllRegisters();

  for (size_t i = 0; i < registerNum; i++) {
    logger->debug("Register {}: 0x{:08x}", i, getRegister(i));
  }

  return true;
}

void AxisCache::invalidate() {
  setRegister(0, 1U << 31);
  logger->info("invalidated AXIS cache.");
}

void AxisCache::setRegister(size_t reg, uint32_t value) {
  if (reg >= registerNum) {
    logger->error("Register index out of range: {}/{}", reg, registerNum);
    throw std::out_of_range("Register index out of range");
  }
  Xil_Out32(getBaseAddr(registerMemory) + REGISTER_OUT(reg), value);
}

uint32_t AxisCache::getRegister(size_t reg) {
  if (reg >= registerNum) {
    logger->error("Register index out of range: {}/{}", reg, registerNum);
    throw std::out_of_range("Register index out of range");
  }
  return Xil_In32(getBaseAddr(registerMemory) + REGISTER_OUT(reg));
}

void AxisCache::resetRegister(size_t reg) { setRegister(reg, 0); }

void AxisCache::resetAllRegisters() {
  for (size_t i = 1; i < registerNum; i++) {
    resetRegister(i);
  }
}

AxisCache::~AxisCache() {}

static char n[] = "axis_cache";
static char d[] = "Register interface VHDL module 'axi_read_cache'";
static char v[] = "xilinx.com:module_ref:axi_read_cache:";
static CorePlugin<AxisCache, n, d, v> f;
