/* Interface for Linux/Unix devices.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <optional>

#include <villas/kernel/devices/driver.hpp>

namespace villas {
namespace kernel {
namespace devices {

class Device {
public:
  virtual ~Device(){};

  virtual std::optional<std::unique_ptr<Driver>> driver() const = 0;
  virtual std::optional<int> iommu_group() const = 0;
  virtual std::string name() const = 0;
  virtual std::filesystem::path override_path() const = 0;
  virtual std::filesystem::path path() const = 0;
  virtual void probe() const = 0;
};

} // namespace devices
} // namespace kernel
} // namespace villas
