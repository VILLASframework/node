/* AXI External Memory Controller (EMC)
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>

#include <villas/fpga/ips/emc.hpp>
#include <villas/plugin.hpp>

using namespace villas::fpga::ip;

bool EMC::init() {
  int ret;
  const uintptr_t base = getBaseAddr(registerMemory);

  const int busWidth = 2;

#if defined(XPAR_XFL_DEVICE_FAMILY_INTEL) && XFL_TO_ASYNCMODE
  // Set Flash to Async mode.
  if (busWidth == 1) {
    WRITE_FLASH_8(base + ASYNC_ADDR, 0x60);
    WRITE_FLASH_8(base + ASYNC_ADDR, 0x03);
  } else if (busWidth == 2) {
    WRITE_FLASH_16(base + ASYNC_ADDR, INTEL_CMD_CONFIG_REG_SETUP);
    WRITE_FLASH_16(base + ASYNC_ADDR, INTEL_CMD_CONFIG_REG_CONFIRM);
  }
#endif

  ret = XFlash_Initialize(&xflash, base, busWidth, 0);
  if (ret != XST_SUCCESS)
    return false;

  return XFlash_IsReady(&xflash);
}

bool EMC::read(uint32_t offset, uint32_t length, uint8_t *data) {
  int ret;

  /* Reset the Flash Device. This clears the ret registers and puts
   * the device in Read mode.
   */
  ret = XFlash_Reset(&xflash);
  if (ret != XST_SUCCESS)
    return false;

  // Perform the read operation.
  ret = XFlash_Read(&xflash, offset, length, data);
  if (ret != XST_SUCCESS)
    return false;

  return false;
}

// objcopy -I ihex -O binary somefile.mcs somefile.bin

bool EMC::flash(uint32_t offset, const std::string &filename) {
  bool result;
  uint32_t length;
  uint8_t *buffer;

  std::ifstream is(filename, std::ios::binary);

  // Get length of file:
  is.seekg(0, std::ios::end);
  length = is.tellg();

  is.seekg(0, std::ios::beg);
  // Allocate memory:

  buffer = new uint8_t[length];
  is.read(reinterpret_cast<char *>(buffer), length);
  is.close();

  result = flash(offset, length, buffer);

  delete[] buffer;

  return result;
}

// Based on xilflash_readwrite_example.c
bool EMC::flash(uint32_t offset, uint32_t length, uint8_t *data) {
  int ret = XST_FAILURE;
  uint32_t start = offset;

  /* Reset the Flash Device. This clears the ret registers and puts
   * the device in Read mode. */
  ret = XFlash_Reset(&xflash);
  if (ret != XST_SUCCESS) {
    return false;
  }

  /* Perform an unlock operation before the erase operation for the Intel
   * Flash. The erase operation will result in an error if the block is
   * locked. */
  if ((xflash.CommandSet == XFL_CMDSET_INTEL_STANDARD) ||
      (xflash.CommandSet == XFL_CMDSET_INTEL_EXTENDED) ||
      (xflash.CommandSet == XFL_CMDSET_INTEL_G18)) {
    ret = XFlash_Unlock(&xflash, offset, 0);
    if (ret != XST_SUCCESS) {
      return false;
    }
  }

  // Perform the Erase operation.
  ret = XFlash_Erase(&xflash, start, length);
  if (ret != XST_SUCCESS) {
    ;
    return false;
  }

  // Perform the Write operation.
  ret = XFlash_Write(&xflash, start, length, data);
  if (ret != XST_SUCCESS) {
    return false;
  }

  // Perform the read operation.
  uint8_t *verify_data = new uint8_t[length];
  ret = XFlash_Read(&xflash, start, length, verify_data);
  if (ret != XST_SUCCESS) {
    delete[] verify_data;
    return false;
  }

  // Compare the data read against the data Written.
  for (unsigned i = 0; i < length; i++) {
    if (verify_data[i] != data[i]) {
      delete[] verify_data;
      return false;
    }
  }

  delete[] verify_data;

  return true;
}

static char n[] = "emc";
static char d[] = "Xilinx's AXI External Memory Controller";
static char v[] = "xilinx.com:ip:axi_emc:";
static CorePlugin<EMC, n, d, v> f;
