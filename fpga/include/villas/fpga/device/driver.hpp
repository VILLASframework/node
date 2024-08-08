/* Driver
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fstream>
#include <iostream>

class Device;

class Driver {
private:
  static constexpr char BIND_DEFAULT[] = "bind";
  static constexpr char UNBIND_DEFAULT[] = "unbind";

public:
  const std::filesystem::path path;

private:
  const std::filesystem::path bind_path;
  const std::filesystem::path unbind_path;
  const std::filesystem::path probe_path;

public:
  Driver(const std::filesystem::path path)
      : Driver(path, path / std::filesystem::path(BIND_DEFAULT),
               path / std::filesystem::path(UNBIND_DEFAULT)){};

  Driver(const std::filesystem::path path,
         const std::filesystem::path bind_path,
         const std::filesystem::path unbind_path)
      : path(path), bind_path(bind_path), unbind_path(unbind_path){};

  std::string name() const;
  void bind(const Device &device) const;
  void unbind(const Device &device) const;
};
