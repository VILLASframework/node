/* Driver for register interface 'registerif'
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2024 Niklas Eiling
 * SPDX-License-Identifier: Apache-2.0
 */

#include <xilinx/xil_io.h>

#include <villas/fpga/ips/register.hpp>

using namespace villas::fpga::ip;

#define REGISTER_RESET (512)
#define REGISTER_OUT(NUM) (4 * NUM)

Register::Register() : Node() {}

bool Register::init() { return true; }

bool Register::check() {

  logger->debug("Checking register interface: Base address: 0x{:08x}",
                getBaseAddr(registerMemory));
  uint32_t buf;
  // we shouldn't change the rate register, because this can lead to hardware fault, so start at 1
  for (size_t i = 1; i < registerNum; i++) {
    setRegister(i, static_cast<uint32_t>(i));
  }

  for (size_t i = 1; i < registerNum; i++) {
    buf = getRegister(i);
    if (buf != i) {
      logger->error("Register {}: 0x{:08x} != 0x{:08x}", i, buf, i);
      return false;
    }
  }

  resetAllRegisters();

  for (size_t i = 0; i < registerNum; i++) {
    logger->debug("Register {}: 0x{:08x}", i, getRegister(i));
  }

  // This is Dino specific for now - we should possibly move this to Dino in the future
  setRegister(0, static_cast<uint32_t>(1000)); // set Dino to a rate of 20 kHz
  setRegister(1, -0.001615254F);
  setRegister(2, 10.8061F);
  uint32_t rate = getRegister(0);
  float scale = getRegisterFloat(1);
  float offset = getRegisterFloat(2);
  logger->info("Check: Register configuration: Rate: {}, Scale: {}, Offset: {}",
               rate, scale, offset);

  return true;
}

void Register::setRegister(size_t reg, uint32_t value) {
  if (reg >= registerNum) {
    logger->error("Register index out of range: {}/{}", reg, registerNum);
    throw std::out_of_range("Register index out of range");
  }
  Xil_Out32(getBaseAddr(registerMemory) + REGISTER_OUT(reg), value);
}

void Register::setRegister(size_t reg, float value) {
  if (reg >= registerNum) {
    logger->error("Register index out of range: {}/{}", reg, registerNum);
    throw std::out_of_range("Register index out of range");
  }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
  Xil_Out32(getBaseAddr(registerMemory) + REGISTER_OUT(reg),
            reinterpret_cast<uint32_t &>(value));
#pragma GCC diagnostic pop
}

uint32_t Register::getRegister(size_t reg) {
  if (reg >= registerNum) {
    logger->error("Register index out of range: {}/{}", reg, registerNum);
    throw std::out_of_range("Register index out of range");
  }
  return Xil_In32(getBaseAddr(registerMemory) + REGISTER_OUT(reg));
}

float Register::getRegisterFloat(size_t reg) {
  if (reg >= registerNum) {
    logger->error("Register index out of range: {}/{}", reg, registerNum);
    throw std::out_of_range("Register index out of range");
  }
  uint32_t value = Xil_In32(getBaseAddr(registerMemory) + REGISTER_OUT(reg));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
  return reinterpret_cast<float &>(value);
#pragma GCC diagnostic pop
}

void Register::resetRegister(size_t reg) {
  if (reg >= registerNum) {
    logger->error("Register index out of range: {}/{}", reg, registerNum);
    throw std::out_of_range("Register index out of range");
  }
  Xil_Out32(getBaseAddr(registerMemory) + REGISTER_RESET, (1 << reg));
}

void Register::resetAllRegisters() {
  Xil_Out32(getBaseAddr(registerMemory) + REGISTER_RESET, 0xFFFFFFFF);
}

Register::~Register() {}

static char n[] = "register";
static char d[] = "Register interface VHDL module 'registerif'";
static char v[] = "xilinx.com:module_ref:registerif:";
static CorePlugin<Register, n, d, v> f;
