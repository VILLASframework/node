/* Node type: Delta Share.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <arrow/table.h>
#include <villas/nodes/delta_sharing/delta_sharing_client.h>
#include <thread>

namespace DeltaSharing {

const std::vector<std::string> ParseURL(std::string path);
std::shared_ptr<DeltaSharingClient>
NewDeltaSharingClient(std::string profile,
                      boost::optional<std::string> cacheLocation);
const std::shared_ptr<arrow::Table> LoadAsArrowTable(std::string path,
                                                     int fileno);
};

// namespace DeltaSharing
