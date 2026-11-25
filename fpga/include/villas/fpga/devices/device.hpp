/* Interface for Linux/Unix devices.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>

#include <villas/fpga/devices/driver.hpp>
#include <villas/fs.hpp>

namespace villas {
namespace fpga {
namespace devices {

class Device {
public:
  virtual ~Device(){};

  virtual std::optional<std::unique_ptr<Driver>> driver() const = 0;
  virtual std::optional<int> iommu_group() const = 0;
  virtual std::string name() const = 0;
  virtual fs::path override_path() const = 0;
  virtual fs::path path() const = 0;
  virtual void probe() const = 0;
};

} // namespace devices
} // namespace fpga
} // namespace villas
