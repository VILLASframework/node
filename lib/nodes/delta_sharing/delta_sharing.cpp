/* Node type: delta_sharing.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <arrow/array.h>
#include <arrow/array/array_base.h>
#include <arrow/array/array_binary.h>
#include <arrow/builder.h>
#include <arrow/scalar.h>
#include <arrow/table.h>
#include <arrow/type.h>
#include <arrow/type_fwd.h>
#include <jansson.h>
#include <parquet/exception.h>

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/delta_sharing/delta_sharing.hpp>
#include <villas/nodes/delta_sharing/functions.hpp>
#include <villas/nodes/delta_sharing/protocol.hpp>
#include <villas/timing.hpp>

#include "villas/log.hpp"

using namespace villas;
using namespace villas::node;

static const char *const OP_READ = "read";
static const char *const OP_WRITE = "write";
static const char *const OP_NOOP = "noop";

int villas::node::deltaSharing_parse(NodeCompat *n, json_t *json) {
  auto *d = n->getData<struct delta_sharing>();

  int ret;
  json_error_t err;

  const char *profilePath = nullptr;
  const char *cacheDir = nullptr;
  const char *tablePath = nullptr;
  const char *op = nullptr;
  const char *schema = nullptr;
  const char *share = nullptr;
  const char *table = nullptr;
  int batch_size = 0;

  ret = json_unpack_ex(
      json, &err, 0,
      "{ s?: s, s?: s, s?: s, s?: s, s?: s, s?: s, s?: s, s?: i }",
      "profile_path", &profilePath, "schema", &schema, "share", &share, "table",
      &table, "cache_dir", &cacheDir, "table_path", &tablePath, "op", &op,
      "batch_size", &batch_size);

  if (ret)
    throw ConfigError(json, err, "node-config-node-delta_sharing");

  if (profilePath)
    d->profilePath = profilePath;
  if (share)
    d->share = share;
  if (schema)
    d->schema = schema;
  if (table)
    d->table = table;
  if (cacheDir)
    d->cacheDir = cacheDir;
  if (tablePath)
    d->tablePath = tablePath;
  if (batch_size > 0)
    d->batchSize = static_cast<size_t>(batch_size);

  if (op) {
    if (strcmp(op, OP_READ) == 0)
      d->table_op = delta_sharing::TableOp::TABLE_READ;
    else if (strcmp(op, OP_WRITE) == 0)
      d->table_op = delta_sharing::TableOp::TABLE_WRITE;
    else
      d->table_op = delta_sharing::TableOp::TABLE_NOOP;
  }

  return 0;
}

char *villas::node::deltaSharing_print(NodeCompat *n) {
  auto *d = n->getData<struct delta_sharing>();

  std::string info =
      std::string("profile_path=") + d->profilePath + ", share =" + d->share +
      ", schema =" + d->schema + ", table =" + d->table +
      ", cache_dir=" + d->cacheDir + ", table_path=" + d->tablePath + ", op=" +
      (d->table_op == delta_sharing::TableOp::TABLE_READ
           ? OP_READ
           : (d->table_op == delta_sharing::TableOp::TABLE_WRITE ? OP_WRITE
                                                                 : OP_NOOP));

  return strdup(info.c_str());
}

int villas::node::deltaSharing_start(NodeCompat *n) {
  auto *d = n->getData<struct delta_sharing>();

  if (d->profilePath.empty())
    throw RuntimeError(
        "'profile_path' must be configured for delta_sharing node");

  std::optional<std::string> cache_opt =
      d->cacheDir.empty() ? std::nullopt
                          : std::optional<std::string>(d->cacheDir);

  d->client = DeltaSharing::NewDeltaSharingClient(d->profilePath, cache_opt);

  if (!d->client)
    throw RuntimeError("Failed to create Delta Sharing client");

  //List all shares from the profile path
  d->shares = d->client->ListShares(100, "");

  const auto &shares = *d->shares;

  for (const auto &share : shares) {
    d->schemas = d->client->ListSchemas(share, 100, "");
    //List all tables in a share
    d->tables = d->client->ListAllTables(share, 100, "");
    //Check if tables are fetched correctly
  }

  return 0;
}

int villas::node::deltaSharing_stop(NodeCompat *n) {
  auto *d = n->getData<struct delta_sharing>();
  d->table_ptr.reset();
  d->tables.reset();
  d->shares.reset();
  d->client.reset();
  return 0;
}

int villas::node::deltaSharing_init(NodeCompat *n) {
  auto *d = n->getData<struct delta_sharing>();

  // d->profile_path = "";
  // d->cache_dir = "";
  // d->tablePath = "";
  d->batchSize = 0;
  d->current_row = 0;

  d->client.reset();
  d->table_ptr.reset();
  d->tables.reset();
  d->shares.reset();
  d->table_op = delta_sharing::TableOp::TABLE_NOOP;

  return 0;
}

int villas::node::deltaSharing_destroy(NodeCompat *n) {
  auto *d = n->getData<struct delta_sharing>();
  d->client.reset();
  if (d->table_ptr != NULL)
    d->table_ptr.reset();
  if (d->tables != NULL)
    d->tables.reset();
  if (d->shares != NULL)
    d->shares.reset();
  return 0;
}

int villas::node::deltaSharing_poll_fds(NodeCompat *n, int fds[]) {
  (void)n;
  (void)fds;
  return -1; // no polling support
}

int villas::node::deltaSharing_read(NodeCompat *n, struct Sample *const smps[],
                                    unsigned cnt) {

  auto *d = n->getData<struct delta_sharing>();

  if (!d->client) {
    n->logger->error("Delta Sharing client not initialized");
    return -1;
  }

  if (d->tablePath.empty()) {
    n->logger->error("No table path configured");
    return -1;
  }

  try {
    auto path = DeltaSharing::ParseURL(d->tablePath);

    if (path.size() != 4) {
      n->logger->error(
          "Invalid table path format. Expected: server#share.schema.table");
      return -1;
    }

    DeltaSharing::DeltaSharingProtocol::Table table;
    table.share = path[1];
    table.schema = path[2];
    table.name = path[3];

    //Get files in the table
    auto files = d->client->ListFilesInTable(table);
    if (!files || files->empty()) {
      n->logger->info("No files found in table");
      return 0;
    }

    for (const auto &f : *files) {
      d->client->PopulateCache(f.url);
    }

    //Load the first file as an Arrow table
    if (!d->table_ptr) {
      d->table_ptr = d->client->LoadAsArrowTable(files->at(0).url);

      if (!d->table_ptr) {
        n->logger->error("Failed to load table from Delta Sharing server");
        return -1;
      }
    }

    unsigned num_rows = d->table_ptr->num_rows();
    unsigned num_cols = d->table_ptr->num_columns();

    auto signals = n->getInputSignals(false);
    if (!signals) {
      return -1;
    }

    unsigned samples_read = 0;
    while (samples_read < cnt && d->current_row < num_rows) {
      auto *smp = smps[samples_read];
      // Set smp length and capacity to the number of columns in the table.
      smp->length = d->table_ptr->num_columns();
      smp->capacity = d->table_ptr->num_columns();
      smp->ts.origin = time_now();
      smp->flags = (int)SampleFlags::HAS_DATA;
      smp->sequence = d->current_row;

      for (unsigned col = 0; col < num_cols && col < signals->size(); col++) {
        auto chunked_array = d->table_ptr->column(col);
        auto scalar_result = chunked_array->GetScalar(d->current_row);

        if (!scalar_result.ok()) {
          n->logger->warn("Failed to get scalar at row {}, col {}: {}",
                          d->current_row, col,
                          scalar_result.status().ToString());
          continue;
        }

        auto scalar = *scalar_result;
        auto sig_type = signals->at(col)->type;

        switch (scalar->type->id()) {
        case arrow::Type::DOUBLE: {
          auto double_scalar =
              std::static_pointer_cast<arrow::DoubleScalar>(scalar)->value;
          smp->data[col].f = double_scalar;
          break;
        }
        case arrow::Type::FLOAT: {
          auto float_scalar =
              std::static_pointer_cast<arrow::FloatScalar>(scalar)->value;
          smp->data[col].f = float_scalar;
          break;
        }
        case arrow::Type::INT64: {
          auto int64_scalar =
              std::static_pointer_cast<arrow::Int64Scalar>(scalar)->value;
          smp->data[col].f = int64_scalar;
          break;
        }
        case arrow::Type::INT32: {
          auto int32_scalar =
              std::static_pointer_cast<arrow::Int32Scalar>(scalar)->value;
          smp->data[col].f = int32_scalar;
          break;
        }
        default:
          n->logger->warn("Unsupported arrow data type for column {}", col);
          if (sig_type == SignalType::FLOAT)
            smp->data[col].f = 0.0;
          else if (sig_type == SignalType::INTEGER)
            smp->data[col].i = 0;
        }
      }
      d->current_row++;
      samples_read++;
    }

    if (samples_read < cnt && d->current_row >= num_rows) {
      n->logger->info("End of table reached at row {}", d->current_row);
    }

    return samples_read;

  } catch (const std::exception &e) {
    n->logger->error("Error reading from Delta Sharing table: {}", e.what());
    return -1;
  }
}

//TODO: write table to delta sharing server. Implementation to be tested
int villas::node::deltaSharing_write(NodeCompat *n, struct Sample *const smps[],
                                     unsigned cnt) {
  auto *d = n->getData<struct delta_sharing>();

  if (!d->client) {
    n->logger->error("Delta Sharing client not initialized");
    return -1;
  }

  if (d->tablePath.empty()) {
    n->logger->error("No table path configured");
    return -1;
  }

  try {
    auto path_parts = DeltaSharing::ParseURL(d->tablePath);
    if (path_parts.size() != 4) {
      n->logger->error(
          "Invalid table path format. Expected: server#share.schema.table");
      return -1;
    }

    auto signals = n->getOutputSignals(false);
    if (!signals) {
      n->logger->error("No output signals configured");
      return -1;
    }

    std::vector<std::shared_ptr<arrow::Array>> arrays;
    std::vector<std::shared_ptr<arrow::Field>> fields;

    for (unsigned col = 0; col < signals->size(); col++) {
      auto signal = signals->at(col);

      std::string field_name = signal->name;
      if (field_name.empty()) {
        field_name = "col_" + std::to_string(col);
      }

      //Determine arrow data type from signal data type
      std::shared_ptr<arrow::DataType> data_type;
      switch (signal->type) {
      case SignalType::FLOAT:
        data_type = arrow::float64();
        break;
      case SignalType::INTEGER:
        data_type = arrow::int64();
        break;
      default:
        data_type = arrow::float64();
      }

      fields.push_back(arrow::field(field_name, data_type));

      //create Arrow array from sampled data
      std::shared_ptr<arrow::Array> array;
      switch (signal->type) {
      case SignalType::FLOAT: {
        std::vector<double> values;
        for (unsigned i = 0; i < cnt; i++) {
          values.push_back(smps[i]->data[col].f);
        }
        arrow::DoubleBuilder builder;
        PARQUET_THROW_NOT_OK(builder.AppendValues(values));
        PARQUET_THROW_NOT_OK(builder.Finish(&array));
        break;
      }
      case SignalType::INTEGER: {
        std::vector<int64_t> values;
        for (unsigned i = 0; i < cnt; i++) {
          values.push_back(smps[i]->data[col].i);
        }
        arrow::Int64Builder builder;
        PARQUET_THROW_NOT_OK(builder.AppendValues(values));
        PARQUET_THROW_NOT_OK(builder.Finish(&array));
        break;
      }
      default:
        n->logger->warn("Unsupported signal type for column {}", col);
        continue;
      }

      arrays.push_back(array);
    }
    // Create Arrow schema and table
    auto schema = std::make_shared<arrow::Schema>(fields);
    auto table = arrow::Table::Make(schema, arrays);

    // Store the table for potential future use
    d->table_ptr = table;

    return cnt;
  } catch (const std::exception &e) {
    n->logger->error("Error writing to Delta Sharing: {}", e.what());
    return -1;
  }
}

static NodeCompatType p;

__attribute__((constructor(110))) static void register_plugin() {
  p.name = "delta_sharing";
  p.description = "Delta Sharing protocol node";
  p.vectorize = 1;
  p.size = sizeof(struct delta_sharing);
  p.init = deltaSharing_init;
  p.destroy = deltaSharing_destroy;
  p.parse = deltaSharing_parse;
  p.print = deltaSharing_print;
  p.start = deltaSharing_start;
  p.stop = deltaSharing_stop;
  p.read = deltaSharing_read;
  p.write = deltaSharing_write;
  p.poll_fds = deltaSharing_poll_fds;

  static NodeCompatFactory ncp(&p);
}
