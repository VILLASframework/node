/* Node type: delta_sharing.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstddef>
#include <optional>

#include <villas/nodes/delta_sharing/functions.hpp>

namespace DeltaSharing {

const std::vector<std::string> ParseURL(std::string path) {
  std::vector<std::string> urlparts;
  std::string url = path;
  auto pos = url.find_last_of('#');

  if (pos == std::string::npos) {
    std::cerr << "Invalid path: " << url << std::endl;
    return std::vector<std::string>();
  }
  urlparts.push_back(url.substr(0, pos));
  url.erase(0, pos + 1);
  while ((pos = url.find(".")) != std::string::npos) {
    urlparts.push_back(url.substr(0, pos));
    url.erase(0, pos + 1);
  }
  urlparts.push_back(url);
  if (urlparts.size() != 4) {
    std::cerr
        << "Path does not follow pattern: <server>#<share>.<schema>.<table>, "
        << path << std::endl;
  }
  return urlparts;
};

std::shared_ptr<DeltaSharingClient>
NewDeltaSharingClient(std::string profile,
                      std::optional<std::string> cacheLocation) {
  return std::make_shared<DeltaSharingClient>(profile, cacheLocation);
};

const std::shared_ptr<arrow::Table> LoadAsArrowTable(std::string path,
                                                     int fileno) {

  auto p = ParseURL(path);
  if (p.size() != 4) {
    std::cerr << "PATH NOT CORRECT: " << path << std::endl;
    return std::shared_ptr<arrow::Table>();
  }
  auto cl = NewDeltaSharingClient(p.at(0), std::nullopt);
  DeltaSharingProtocol::Table t;
  t.name = p.at(3);
  t.schema = p.at(2);
  t.share = p.at(1);

  auto flist = cl->ListFilesInTable(t);
  std::vector<std::thread> writethreads;

  for (long unsigned int i = 0; i < flist->size(); i++) {
    auto arg = flist->at(i).url;
    std::thread th(&DeltaSharingClient::PopulateCache, cl, arg);
    writethreads.push_back(std::move(th));
  }

  for (auto i = writethreads.begin(); i != writethreads.end(); i++) {
    if (i->joinable()) {
      i->join();
    }
  }

  if (flist->size() > static_cast<size_t>(fileno)) {
    auto f = flist->at(fileno);
    std::cerr << "Number of threads supported: " << cl->GetNumberOfThreads()
              << std::endl;

    return cl->LoadAsArrowTable(f.url);
  } else
    return std::shared_ptr<arrow::Table>();
};

}; // namespace DeltaSharing
