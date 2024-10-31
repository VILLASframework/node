/* Linux/Unix device which represents an IP component of a FPGA.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <villas/kernel/devices/platform_device.hpp>

namespace villas {
namespace kernel {
namespace devices {

class IpDevice : public PlatformDevice {
public:
  static IpDevice from(const std::filesystem::path unsafe_path);
  static bool is_path_valid(const std::filesystem::path unsafe_path);

private:
  IpDevice() = delete;
  IpDevice(
      const std::filesystem::path valid_path) //! Dont allow unvalidated paths
      : PlatformDevice(valid_path){};

public:
  size_t addr() const;
  std::string ip_name() const;
};

} // namespace devices
} // namespace kernel
} // namespace villas
