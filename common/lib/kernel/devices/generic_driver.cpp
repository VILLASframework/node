/* GenericDriver
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/kernel/devices/generic_driver.hpp>

#include <villas/kernel/devices/device.hpp>
#include <villas/utils.hpp>

using villas::kernel::devices::Device, villas::kernel::devices::GenericDriver;
using villas::utils::write_to_file;

void GenericDriver::attach(const Device &device) const {
  if (device.driver().has_value()) {
    device.driver().value()->unbind(device);
  }
  this->override(device);
  device.probe();
}

void GenericDriver::bind(const Device &device) const {
  write_to_file(device.name(), this->bind_path);
}

std::string GenericDriver::name() const {
  size_t pos = path.u8string().rfind('/');
  return path.u8string().substr(pos + 1);
}

void GenericDriver::override(const Device &device) const {
  write_to_file(this->name(), device.override_path());
}

void GenericDriver::unbind(const Device &device) const {
  write_to_file(device.name(), this->unbind_path);
}
