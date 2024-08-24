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
  void attach(const Device &device) const override;
  void bind(const Device &device) const override;
  std::string name() const override;
  void override(const Device &device) const override;
  void unbind(const Device &device) const override;
};

} // namespace devices
} // namespace kernel
} // namespace villas