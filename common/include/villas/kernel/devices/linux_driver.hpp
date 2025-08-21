/* Implementation of driver interface for Linux/Unix based operation system drivers.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fstream>
#include <iostream>

#include <villas/fs.hpp>
#include <villas/kernel/devices/driver.hpp>

namespace villas {
namespace kernel {
namespace devices {

class LinuxDriver : public Driver {
private:
  static constexpr char BIND_DEFAULT[] = "bind";
  static constexpr char UNBIND_DEFAULT[] = "unbind";

public:
  const fs::path path;

private:
  const fs::path bind_path;
  const fs::path unbind_path;

public:
  LinuxDriver(const fs::path path)
      : LinuxDriver(path, path / fs::path(BIND_DEFAULT),
                    path / fs::path(UNBIND_DEFAULT)) {};

  LinuxDriver(const fs::path path, const fs::path bind_path,
              const fs::path unbind_path)
      : path(path), bind_path(bind_path), unbind_path(unbind_path) {};

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
