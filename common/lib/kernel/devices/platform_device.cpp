/* Platform bus based Linux/Unix device.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/kernel/devices/platform_device.hpp>
#include <villas/kernel/devices/generic_driver.hpp>

using villas::kernel::devices::Driver, villas::kernel::devices::GenericDriver;
using villas::kernel::devices::PlatformDevice;
using villas::kernel::devices::utils::write_to_file;

std::optional<std::unique_ptr<Driver>> PlatformDevice::driver() const {
  std::filesystem::path driver_symlink =
      this->m_path / std::filesystem::path("driver");

  if (!std::filesystem::is_symlink(driver_symlink))
    return std::nullopt;

  std::filesystem::path driver_path =
      std::filesystem::canonical(driver_symlink);
  return std::make_optional(std::make_unique<GenericDriver>(driver_path));
}

std::optional<int> PlatformDevice::iommu_group() const {
  std::filesystem::path symlink =
      std::filesystem::path(this->m_path.u8string() + "/iommu_group");

  std::filesystem::path link = std::filesystem::read_symlink(symlink);
  std::string delimiter = "iommu_groups/";
  int pos = link.u8string().find(delimiter);
  int iommu_group = std::stoi(link.u8string().substr(pos + delimiter.length()));
  return std::make_optional<int>(iommu_group);
}

std::filesystem::path PlatformDevice::path() const { return this->m_path; };

void PlatformDevice::probe() const {
  write_to_file(this->name(), this->m_probe_path);
}

std::filesystem::path PlatformDevice::override_path() const {
  return this->m_override_path;
}

std::string PlatformDevice::name() const {
  size_t pos = this->m_path.u8string().rfind('/');
  return this->m_path.u8string().substr(pos + 1);
}
