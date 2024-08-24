/* Utils
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <villas/kernel/devices/utils.hpp>
#include <villas/log.hpp>

namespace utils = villas::kernel::devices::utils;

void utils::write_to_file(std::string data, const std::filesystem::path file) {
  villas::Log::get("Filewriter")->debug("{} > {}", data, file.u8string());
  std::ofstream outputFile(file.u8string());

  if (outputFile.is_open()) {
    // Write to file
    outputFile << data;
    outputFile.close();
  } else {
    throw std::filesystem::filesystem_error("Cannot open outputfile",
                                            std::error_code());
  }
}

std::vector<std::string>
utils::read_names_in_directory(const std::string &name) {
  DIR *directory = opendir(name.c_str());

  struct dirent *dp;
  std::vector<std::string> names;
  dp = readdir(directory);
  while (dp != NULL) {
    auto name = std::string(dp->d_name);
    if (name != "." && name != "..") {
      names.push_back(name);
    }
    dp = readdir(directory);
  }
  closedir(directory);
  return names;
}