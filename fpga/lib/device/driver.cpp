/* Driver
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/fpga/devices/utils.hpp>
#include <villas/fpga/devices/ip_device.hpp>

std::string Driver::name() const {
  size_t pos = path.u8string().rfind('/');
  return path.u8string().substr(pos + 1);
}

void Driver::bind(const Device &device) const {
  write_to_file(device.name(), this->bind_path);
};

void Driver::unbind(const Device &device) const {
  write_to_file(device.name(), this->unbind_path);
};

