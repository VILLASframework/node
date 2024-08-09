/* IpDevice
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <villas/fpga/devices/device.hpp>

class IpDevice : public Device {
public:
  static IpDevice from(const std::filesystem::path unsafe_path);
  static bool is_path_valid(const std::filesystem::path unsafe_path);

private:
  IpDevice(const std::filesystem::path valid_path) : Device(valid_path){};

public:
  std::string ip_name() const;
  size_t addr() const;
  int iommu_group() const;
};
