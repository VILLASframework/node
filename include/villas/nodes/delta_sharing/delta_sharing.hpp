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

#include <villas/nodes/delta_sharing/Protocol.h>
#include <villas/nodes/delta_sharing/DeltaSharingClient.h>

namespace arrow { class Table; }

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;

struct delta_sharing {
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

char *deltaSharing_print(NodeCompat *n);

int deltaSharing_parse(NodeCompat *n, json_t *json);

int deltaSharing_start(NodeCompat *n);

int deltaSharing_stop(NodeCompat *n);

int deltaSharing_init(NodeCompat *n);

int deltaSharing_destroy(NodeCompat *n);

int deltaSharing_poll_fds(NodeCompat *n, int fds[]);

int deltaSharing_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int deltaSharing_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

} // namespace node
} // namespace villas
