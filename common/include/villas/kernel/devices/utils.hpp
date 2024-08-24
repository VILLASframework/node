/* Utils
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace villas {
namespace kernel {
namespace devices {
namespace utils {

void write_to_file(std::string data, const std::filesystem::path file);
std::vector<std::string> read_names_in_directory(const std::string &name);

} // namespace utils
} // namespace devices
} // namespace kernel
} // namespace villas
