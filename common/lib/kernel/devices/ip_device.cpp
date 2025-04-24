/* Linux/Unix device which represents an IP component of a FPGA.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <filesystem>
#include <regex>
#include <stdexcept>

#include <villas/exceptions.hpp>
#include <villas/kernel/devices/ip_device.hpp>
#include <villas/utils.hpp>

using villas::kernel::devices::IpDevice;

IpDevice IpDevice::from(const std::filesystem::path unsafe_path) {
  if (!is_path_valid(unsafe_path))
    throw RuntimeError(
        "Path {} failed validation as IpDevicePath [adress in hex].[name] ",
        unsafe_path.u8string());
  return IpDevice(unsafe_path);
}

std::string IpDevice::ip_name() const {
  int pos = name().find('.');
  return name().substr(pos + 1);
}

size_t IpDevice::addr() const {
  size_t pos = name().find('.');
  std::string addr_hex = name().substr(0, pos);

  // Convert from hex-string to number
  std::stringstream ss;
  ss << std::hex << addr_hex;
  size_t addr = 0;
  ss >> addr;

  return addr;
}

bool IpDevice::is_path_valid(const std::filesystem::path unsafe_path) {
  std::string assumed_device_name = unsafe_path.filename();

  // Match format of hexaddr.devicename
  if (!std::regex_match(assumed_device_name,
                        std::regex(R"([0-9A-Fa-f]+\..*)"))) {
    return false;
  }

  return true;
}

std::vector<villas::kernel::devices::IpDevice>
IpDevice::from_directory(std::filesystem::path devices_directory) {
  std::vector<villas::kernel::devices::IpDevice> devices;

  const std::vector<std::string> devicetree_names =
      villas::utils::read_names_in_directory(devices_directory);

  for (auto devicetree_name : devicetree_names) {
    auto path_to_device =
        devices_directory / std::filesystem::path(devicetree_name);
    try {
      auto device = villas::kernel::devices::IpDevice::from(path_to_device);
      devices.push_back(device);
    } catch (std::runtime_error &e) {
    }
  }
  return devices;
}