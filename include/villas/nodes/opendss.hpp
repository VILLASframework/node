/* OpenDSS node-type for electric power distribution system simulator OpenDSS.
 *
 * Author: Jitpanu Maneeratpongsuk <jitpanu.maneeratpongsuk@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <unordered_set>

#include <villas/format.hpp>
#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/node_compat.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class OpenDSS : public Node {
protected:
  enum ElementType { generator, load, monitor, isource };

  struct Element {
    std::string
        name; // Name of the element must be that same as declared in OpenDSS file
    ElementType type; // Type of the element
    std::vector<int>
        mode; // Mode is set according to the function mode in OpenDSS
  };

  bool writingTurn;
  const char *path;
  timespec ts;

  std::vector<Element> dataIn; // Vector of elements to be written.
  std::vector<std::string> monitorNames;
  std::unordered_set<std::string> loads, generators, monitors,
      isource_set; // Set of corresponding element type.

  pthread_mutex_t mutex;
  pthread_cond_t cv;

  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;

  void parseData(json_t *json, bool in);

  void getElementName(ElementType type, std::unordered_set<std::string> *set);

  int extractMonitorData(struct Sample *const *smps);

  std::string dssPutCommand(const std::string &cmd) const;

public:
  OpenDSS(const uuid_t &id = {}, const std::string &name = "");

  virtual ~OpenDSS();

  int prepare() override;

  int parse(json_t *json) override;

  int start() override;

  int stop() override;
};

} // namespace node
} // namespace villas
