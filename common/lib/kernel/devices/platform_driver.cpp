/* PlatformDriver
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/kernel/devices/platform_device.hpp>
#include <villas/kernel/devices/utils.hpp>

using villas::kernel::devices::PlatformDevice,
    villas::kernel::devices::PlatformDriver;
using villas::kernel::devices::utils::write_to_file;

std::string PlatformDriver::name() const {
  size_t pos = path.u8string().rfind('/');
  return path.u8string().substr(pos + 1);
}

void PlatformDriver::bind(const Device &device) const {
  write_to_file(device.name(), this->bind_path);
};

void PlatformDriver::unbind(const Device &device) const {
  write_to_file(device.name(), this->unbind_path);
};

void PlatformDriver::override(const Device &device) const {
  write_to_file(this->name(), device.override_path());
};

void PlatformDriver::attach(const Device &device) const {
  if (device.driver().has_value()) {
    device.driver().value()->unbind(device);
  }
  this->override(device);
  device.probe();
};