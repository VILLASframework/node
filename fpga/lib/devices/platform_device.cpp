/* Platform bus based Linux/Unix device.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/fpga/devices/linux_driver.hpp>
#include <villas/fpga/devices/platform_device.hpp>
#include <villas/utils.hpp>

using villas::fpga::devices::Driver, villas::fpga::devices::LinuxDriver;
using villas::fpga::devices::PlatformDevice;
using villas::utils::write_to_file;

std::optional<std::unique_ptr<Driver>> PlatformDevice::driver() const {
  fs::path driver_symlink = this->m_path / fs::path("driver");

  if (!fs::is_symlink(driver_symlink))
    return std::nullopt;

  fs::path driver_path = fs::canonical(driver_symlink);
  return std::make_optional(std::make_unique<LinuxDriver>(driver_path));
}

std::optional<int> PlatformDevice::iommu_group() const {
  fs::path symlink = fs::path(this->m_path.string() + "/iommu_group");

  fs::path link = fs::read_symlink(symlink);
  std::string delimiter = "iommu_groups/";
  int pos = link.string().find(delimiter);
  int iommu_group = std::stoi(link.string().substr(pos + delimiter.length()));
  return std::make_optional<int>(iommu_group);
}

fs::path PlatformDevice::path() const { return this->m_path; };

void PlatformDevice::probe() const {
  write_to_file(this->name(), this->m_probe_path);
}

fs::path PlatformDevice::override_path() const { return this->m_override_path; }

std::string PlatformDevice::name() const { return this->m_path.filename(); }
