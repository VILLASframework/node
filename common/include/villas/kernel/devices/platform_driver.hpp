/* PlatformDriver
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fstream>
#include <iostream>

namespace villas {
namespace kernel {
namespace devices {

class PlatformDevice;

class PlatformDriver {
private:
  static constexpr char BIND_DEFAULT[] = "bind";
  static constexpr char UNBIND_DEFAULT[] = "unbind";

public:
  const std::filesystem::path path;

private:
  const std::filesystem::path bind_path;
  const std::filesystem::path unbind_path;

public:
  PlatformDriver(const std::filesystem::path path)
      : PlatformDriver(path, path / std::filesystem::path(BIND_DEFAULT),
                       path / std::filesystem::path(UNBIND_DEFAULT)){};

  PlatformDriver(const std::filesystem::path path,
                 const std::filesystem::path bind_path,
                 const std::filesystem::path unbind_path)
      : path(path), bind_path(bind_path), unbind_path(unbind_path){};

  std::string name() const;
  void attach(const PlatformDevice &device) const;

private:
  void bind(const PlatformDevice &device) const;
  void unbind(const PlatformDevice &device) const;
  void override(const PlatformDevice &device) const;
};

} // namespace devices
} // namespace kernel
} // namespace villas