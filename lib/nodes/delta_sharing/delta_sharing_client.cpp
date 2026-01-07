/* Node type: delta_sharing.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <thread>

#include <arrow/api.h>
#include <arrow/dataset/dataset.h>
#include <arrow/filesystem/filesystem.h>
#include <arrow/io/api.h>
#include <arrow/io/memory.h>
#include <arrow/type_fwd.h>
#include <parquet/arrow/reader.h>
#include <parquet/exception.h>
#include <parquet/stream_reader.h>

#include <villas/nodes/delta_sharing/delta_sharing_client.hpp>

namespace DeltaSharing {
DeltaSharingClient::DeltaSharingClient(const std::string &filename,
                                       std::optional<std::string> cacheLocation)
    : restClient(filename) {
  auto path = std::filesystem::current_path().generic_string();
  std::cerr << "Current path: " << path << std::endl;
  path.append("/cache");
  this->cacheLocation = cacheLocation.value_or(path);
  if (std::filesystem::exists(this->cacheLocation) == false)
    std::filesystem::create_directories(this->cacheLocation);

  if (std::filesystem::exists(this->cacheLocation) &&
      std::filesystem::is_directory(this->cacheLocation)) {
    auto p = std::filesystem::status(this->cacheLocation).permissions();
    std::cerr << "Cache directory:" << this->cacheLocation << " Permission: "
              << ((p & std::filesystem::perms::owner_read) !=
                          std::filesystem::perms::none
                      ? "r"
                      : "-")
              << ((p & std::filesystem::perms::owner_write) !=
                          std::filesystem::perms::none
                      ? "w"
                      : "-")
              << ((p & std::filesystem::perms::owner_exec) !=
                          std::filesystem::perms::none
                      ? "x"
                      : "-")
              << ((p & std::filesystem::perms::group_read) !=
                          std::filesystem::perms::none
                      ? "r"
                      : "-")
              << ((p & std::filesystem::perms::group_write) !=
                          std::filesystem::perms::none
                      ? "w"
                      : "-")
              << ((p & std::filesystem::perms::group_exec) !=
                          std::filesystem::perms::none
                      ? "x"
                      : "-")
              << ((p & std::filesystem::perms::others_read) !=
                          std::filesystem::perms::none
                      ? "r"
                      : "-")
              << ((p & std::filesystem::perms::others_write) !=
                          std::filesystem::perms::none
                      ? "w"
                      : "-")
              << ((p & std::filesystem::perms::others_exec) !=
                          std::filesystem::perms::none
                      ? "x"
                      : "-")
              << '\n';
  }
  this->maxThreads = std::thread::hardware_concurrency();
};

std::shared_ptr<arrow::Table>
DeltaSharingClient::LoadAsArrowTable(std::string &url) {

  if (url.length() == 0)
    return std::shared_ptr<arrow::Table>();

  int protocolLength = 0;
  if ((url.find("http://")) != std::string::npos) {
    protocolLength = 7;
  }

  if ((url.find("https://")) != std::string::npos) {
    protocolLength = 8;
  }
  auto pos = url.find_first_of('?', protocolLength);
  auto path =
      url.substr(protocolLength, pos - protocolLength); // Removing "https://"

  std::vector<std::string> urlparts;
  while ((pos = path.find("/")) != std::string::npos) {
    urlparts.push_back(path.substr(0, pos));
    path.erase(0, pos + 1);
  }
  if (urlparts.size() != 3) {
    std::cerr << "Invalid URL:" << url << std::endl;
    return std::shared_ptr<arrow::Table>();
  }
  std::string tbl = urlparts.back();
  urlparts.pop_back();
  std::string schema = urlparts.back();
  urlparts.pop_back();
  std::string share = urlparts.back();

  auto completePath =
      this->cacheLocation + "/" + share + "/" + schema + "/" + tbl;
  std::shared_ptr<arrow::io::ReadableFile> infile;
  try {
    PARQUET_ASSIGN_OR_THROW(
        infile, arrow::io::ReadableFile::Open(completePath + "/" + path));
  } catch (parquet::ParquetStatusException &e) {
    std::cerr << "error code:(" << e.status() << ") Message: " << e.what()
              << std::endl;
    return std::shared_ptr<arrow::Table>();
  }

  /*         auto reader_result = parquet::arrow::OpenFile(infile, arrow::default_memory_pool());
        if (!reader_result.ok()) {
          throw std::runtime_error(reader_result.status().ToString());
        }
        std::unique_ptr<parquet::arrow::FileReader> reader = std::move(reader_result).ValueOrDie();
        std::shared_ptr<arrow::Table> table;

        PARQUET_THROW_NOT_OK(reader->ReadTable(&table)); */

  std::unique_ptr<parquet::arrow::FileReader> reader;
  PARQUET_THROW_NOT_OK(
      parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader));
  std::shared_ptr<arrow::Table> table;
  PARQUET_THROW_NOT_OK(reader->ReadTable(&table));

  return table;
};

std::shared_ptr<arrow::Table>
DeltaSharingClient::ReadTableFromCache(std::string &completePath) {
  // To be tested on tables downloaded from cloud

  /* fs::FileSelector selector;
       selector.base_dir = completePath;


       auto filesystem = fs::FileSystemFromUriOrPath(completePath);
        auto format = std::make_shared<ds::ParquetFileFormat>();
        auto format = std::make_shared<ds::ParquetFileFormat>();
       ds::FileSystemFactoryOptions opts;

        ds::FileSystemDatasetFactory
       ds::FileSystemDatasetFactory f;

       auto factor = ds::FileSystemDatasetFactory::Make(filesystem, selector, NULL, NULL); */
  return std::shared_ptr<arrow::Table>();
};

const std::shared_ptr<std::vector<DeltaSharingProtocol::Share>>
DeltaSharingClient::ListShares(int maxResult,
                               const std::string &pageToken) const {
  return this->restClient.ListShares(maxResult, pageToken);
};

const std::shared_ptr<std::vector<DeltaSharingProtocol::Schema>>
DeltaSharingClient::ListSchemas(const DeltaSharingProtocol::Share &share,
                                int maxResult,
                                const std::string &pageToken) const {
  return this->restClient.ListSchemas(share, maxResult, pageToken);
};

const std::shared_ptr<std::vector<DeltaSharingProtocol::Table>>
DeltaSharingClient::ListTables(const DeltaSharingProtocol::Schema &schema,
                               int maxResult,
                               const std::string &pageToken) const {
  return this->restClient.ListTables(schema, maxResult, pageToken);
};

const std::shared_ptr<std::vector<DeltaSharingProtocol::Table>>
DeltaSharingClient::ListAllTables(const DeltaSharingProtocol::Share &share,
                                  int maxResult,
                                  const std::string &pageToken) const {
  return this->restClient.ListAllTables(share, maxResult, pageToken);
};

const std::shared_ptr<std::vector<DeltaSharingProtocol::File>>
DeltaSharingClient::ListFilesInTable(
    const DeltaSharingProtocol::Table &table) const {
  return this->restClient.ListFilesInTable(table);
};

const DeltaSharingProtocol::Metadata DeltaSharingClient::QueryTableMetadata(
    const DeltaSharingProtocol::Table &table) const {
  return this->restClient.QueryTableMetadata(table);
};
}; // namespace DeltaSharing
