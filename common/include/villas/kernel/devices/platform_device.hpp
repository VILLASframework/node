/* Platform bus based Linux/Unix device.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/fs.hpp>
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
  const fs::path m_path;
  const fs::path m_probe_path;
  const fs::path m_override_path;

public:
  PlatformDevice(const fs::path path)
      : PlatformDevice(path, fs::path(PROBE_DEFAULT),
                       path / fs::path(OVERRIDE_DEFAULT)) {};

  PlatformDevice(const fs::path path, const fs::path probe_path,
                 const fs::path override_path)
      : m_path(path), m_probe_path(probe_path),
        m_override_path(override_path) {};

  // Implement device interface
  std::optional<std::unique_ptr<Driver>> driver() const override;
  std::optional<int> iommu_group() const override;
  std::string name() const override;
  fs::path override_path() const override;
  fs::path path() const override;
  void probe() const override;
};

} // namespace devices
} // namespace kernel
} // namespace villas
