/* Device Tree Reader for IpDevices
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <dirent.h>
#include <stdexcept>
#include <sys/types.h>
#include <vector>

#include <filesystem>

#include <villas/kernel/devices/ip_device.hpp>
#include <villas/utils.hpp>

class IpDeviceReader {
public:
  std::vector<villas::kernel::devices::IpDevice>
  read(std::filesystem::path devices_directory) const {
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
};
