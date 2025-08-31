/* Node type: Delta Share.
 *
 * Author:
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <jansson.h>

#include "villas/nodes/delta_share/Protocol.h"
#include "villas/nodes/delta_share/DeltaSharingClient.h"

namespace arrow { class Table; }

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;

struct delta_share {
  // Configuration
  std::string profile_path;
  std::string cache_dir;
  std::string table_path;
  size_t batch_size;
  std::string schema;
  std::string share;
  std::string table;

  // Client and state
  std::shared_ptr<DeltaSharing::DeltaSharingClient> client;
  std::shared_ptr<std::vector<DeltaSharing::DeltaSharingProtocol::Schema>> schemas;
  std::shared_ptr<arrow::Table> table_ptr;
  std::shared_ptr<std::vector<DeltaSharing::DeltaSharingProtocol::Table>> tables;
  std::shared_ptr<std::vector<DeltaSharing::DeltaSharingProtocol::Share>> shares;

  enum class TableOp { TABLE_NOOP, TABLE_READ, TABLE_WRITE } table_op;
};

char *deltaShare_print(NodeCompat *n);

int deltaShare_parse(NodeCompat *n, json_t *json);

int deltaShare_start(NodeCompat *n);

int deltaShare_stop(NodeCompat *n);

int deltaShare_init(NodeCompat *n);

int deltaShare_destroy(NodeCompat *n);

int deltaShare_poll_fds(NodeCompat *n, int fds[]);

int deltaShare_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int deltaShare_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

} // namespace node
} // namespace villas
