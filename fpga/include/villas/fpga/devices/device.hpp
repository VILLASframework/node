/* Device
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <optional>
#include <villas/fpga/devices/driver.hpp>
#include <villas/fpga/devices/utils.hpp>

class Device {
private:
  static constexpr char PROBE_DEFAULT[] = "/sys/bus/platform/drivers_probe";
  static constexpr char OVERRIDE_DEFAULT[] = "driver_override";

public:
  const std::filesystem::path path;
  const std::filesystem::path probe_path;
  const std::filesystem::path override_path;
  
public:
  Device(const std::filesystem::path path)
      : Device(path, std::filesystem::path(PROBE_DEFAULT),
               path / std::filesystem::path(OVERRIDE_DEFAULT)){};
  Device(const std::filesystem::path path,
         const std::filesystem::path probe_path,
         const std::filesystem::path override_path)
      : path(path), probe_path(probe_path), override_path(override_path){};

  std::string name() const {
    size_t pos = path.u8string().rfind('/');
    return path.u8string().substr(pos + 1);
  }

  std::optional<Driver> driver() const {
    std::filesystem::path driver_symlink =
        this->path / std::filesystem::path("driver");

    if (!std::filesystem::is_symlink(driver_symlink))
      return std::nullopt;

    std::filesystem::path driver_path =
        std::filesystem::canonical(driver_symlink);
    return Driver(driver_path);
  };

  void probe() const { write_to_file(this->name(), this->probe_path); };
};
