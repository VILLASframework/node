/* Node type: Delta Sharing.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace DeltaSharing {

namespace DeltaSharingProtocol {

using json = nlohmann::json;

struct DeltaSharingProfile {
public:
  int shareCredentialsVersion;
  std::string endpoint;
  std::string bearerToken;
  std::optional<std::string> expirationTime;
};

inline void from_json(const json &j, DeltaSharingProfile &p) {
  if (j.contains("shareCredentialsVersion")) {
    p.shareCredentialsVersion = j["shareCredentialsVersion"];
  }
  if (j.contains("endpoint")) {
    p.endpoint = j["endpoint"];
  }
  if (j.contains("bearerToken")) {
    p.bearerToken = j["bearerToken"];
  }
  if (j.contains("expirationTime")) {
    p.expirationTime = j["expirationTime"];
  }
};

struct Share {
public:
  std::string name = "";
  std::optional<std::string> id;
};

inline void from_json(const json &j, Share &s) {
  s.name = j["name"];
  if (j.contains("id") == false) {
    s.id = std::nullopt;
  } else {
    s.id = j["id"];
  }
};

struct Schema {
public:
  std::string name;
  std::string share;
};

inline void from_json(const json &j, Schema &s) {
  s.name = j["name"];
  if (j.contains("share") == true) {
    s.share = j["share"];
  }
};

struct Table {
public:
  std::string name;
  std::string share;
  std::string schema;
};

inline void from_json(const json &j, Table &t) {
  if (j.contains("name")) {
    t.name = j["name"];
  }
  if (j.contains("share")) {
    t.share = j["share"];
  }
  if (j.contains("schema")) {
    t.schema = j["schema"];
  }
};

struct File {
public:
  std::string url;
  std::optional<std::string> id;
  std::map<std::string, std::string> partitionValues;
  std::size_t size;
  std::string stats;
  std::optional<std::string> timestamp;
  std::optional<std::string> version;
};

inline void from_json(const json &j, File &f) {
  if (j.contains("url")) {
    f.url = j["url"];
  }
  if (j.contains("id")) {
    f.id = j["id"];
  }
  if (j.contains("partitionValues")) {
    json arr = j["partitionValues"];
    auto f2 = f.partitionValues;
    if (arr.is_array()) {
      for (auto it = arr.begin(); it < arr.end(); it++) {
        f2.insert({it.key(), it.value()});
      }
    }
  }
  if (j.contains("size")) {
    f.size = j["size"];
  }
  if (j.contains("stats")) {
    f.stats = j["stats"];
  }
  if (j.contains("timestamp")) {
    f.timestamp = j["timestamp"];
  }
  if (j.contains("version")) {
    f.version = j["version"];
  }
};

struct Format {
  std::string provider;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Format, provider)

struct Metadata {
  Format format;
  std::string id;
  std::vector<std::string> partitionColumns;
  std::string schemaString;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Metadata, format, id, partitionColumns,
                                   schemaString)

struct data {
  std::vector<std::string> predicateHints;
  int limitHint;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(data, predicateHints, limitHint)

struct format {
  std::string provider;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(format, provider)

struct protocol {
  int minReaderVersion;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(protocol, minReaderVersion)

struct stats {
  long long numRecords;
  long minValues;
  long maxValues;
  long nullCount;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(stats, numRecords, minValues, maxValues,
                                   nullCount)

}; // namespace DeltaSharingProtocol
}; // namespace DeltaSharing
