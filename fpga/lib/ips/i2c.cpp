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

using namespace villas::fpga::ip;

I2c::I2c() : Node("i2c") {}

I2c::~I2c() {}

bool I2c::init() override {}

bool I2c::reset() override {}

bool I2c::write(std::list<u8> &data) {}

bool I2c::read(std::list<u8> &data, size_t max_read) {}

void I2cFactory::parse(Core &ip, json_t *cfg) {
  NodeFactory::parse(ip, cfg);

  auto &i2c = dynamic_cast<I2c &>(ip);

  int i2c_frequency = 0;

  json_error_t err;
  int ret = json_unpack_ex(
      cfg, &err, 0, "{ s: { s?: i, s?: i, s?: i, s?: i, s?: i} }", "parameters",
      "c_iic_freq", &i2c_frequency, "c_ten_bit_adr", &i2c.xConfig.Has10BitAddr,
      "c_gpo_width", &i2c.xConfig.GpOutWidth, "component_name",
      &i2c.xConfig.Name, "c_baseaddr", &i2c.xConfig.BaseAddress);
  if (ret != 0)
    throw ConfigError(cfg, err, "", "Failed to parse DMA configuration");

  dma.configDone = true;
}

static I2cFactory f;
