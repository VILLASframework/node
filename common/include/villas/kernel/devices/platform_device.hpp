/* Platform bus based Linux/Unix device.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <villas/kernel/devices/device.hpp>
#include <villas/kernel/devices/driver.hpp>

namespace villas {
namespace kernel {
namespace devices {

class PlatformDevice : public Device {
private:
  static constexpr char PROBE_DEFAULT[] = "/sys/bus/platform/drivers_probe";
  static constexpr char OVERRIDE_DEFAULT[] = "driver_override";

private:
  const std::filesystem::path m_path;
  const std::filesystem::path m_probe_path;
  const std::filesystem::path m_override_path;

public:
  PlatformDevice(const std::filesystem::path path)
      : PlatformDevice(path, std::filesystem::path(PROBE_DEFAULT),
                       path / std::filesystem::path(OVERRIDE_DEFAULT)){};

  PlatformDevice(const std::filesystem::path path,
                 const std::filesystem::path probe_path,
                 const std::filesystem::path override_path)
      : m_path(path), m_probe_path(probe_path),
        m_override_path(override_path){};

  // Implement device interface
  std::optional<std::unique_ptr<Driver>> driver() const override;
  std::optional<int> iommu_group() const override;
  std::string name() const override;
  std::filesystem::path override_path() const override;
  std::filesystem::path path() const override;
  void probe() const override;
};

} // namespace devices
} // namespace kernel
} // namespace villas
