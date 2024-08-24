/* GenericDriver - Works for standard Linux drivers
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <villas/kernel/devices/driver.hpp>

namespace villas {
namespace kernel {
namespace devices {

class GenericDriver : public Driver {
private:
  static constexpr char BIND_DEFAULT[] = "bind";
  static constexpr char UNBIND_DEFAULT[] = "unbind";

public:
  const std::filesystem::path path;

private:
  const std::filesystem::path bind_path;
  const std::filesystem::path unbind_path;

public:
  GenericDriver(const std::filesystem::path path)
      : GenericDriver(path, path / std::filesystem::path(BIND_DEFAULT),
                      path / std::filesystem::path(UNBIND_DEFAULT)){};

  GenericDriver(const std::filesystem::path path,
                const std::filesystem::path bind_path,
                const std::filesystem::path unbind_path)
      : path(path), bind_path(bind_path), unbind_path(unbind_path){};

public:
  virtual void attach(const Device &device) const = 0;
  virtual void bind(const Device &device) const = 0;
  virtual std::string name() const = 0;
  virtual void override(const Device &device) const = 0;
  virtual void unbind(const Device &device) const = 0;
};

} // namespace devices
} // namespace kernel
} // namespace villas