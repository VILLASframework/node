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
#include <villas/kernel/devices/utils.hpp>

class IpDeviceReader {
public:
  const std::vector<std::string> devicetree_names;
  std::vector<villas::kernel::devices::IpDevice> devices;

  IpDeviceReader(std::filesystem::path devices_directory)
      : devicetree_names(
            villas::kernel::devices::utils::read_names_in_directory(
                devices_directory)) {
    for (auto devicetree_name : this->devicetree_names) {
      auto path_to_device =
          devices_directory / std::filesystem::path(devicetree_name);
      try {
        auto device = villas::kernel::devices::IpDevice::from(path_to_device);
        this->devices.push_back(device);
      } catch (std::runtime_error &e) {
      }
    }
  }
};
