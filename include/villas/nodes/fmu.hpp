/* Node type: Functional Mock-up Unit.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>, Jitpanu Maneeratpongsuk <jitpanu.maneeratpongsuk@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <ctime>
#include <string>
#include <vector>

#include <fmilib.h>

#include <villas/format.hpp>
#include <villas/node.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

struct FmuSignal {
  unsigned int ref = 0;
  fmi3_base_type_enu_t type = fmi3_base_type_enu_t::fmi3_base_type_float64;
  std::string name;
};

class FmuNode : public Node {

protected:
  int parse(json_t *json) override;

  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;

  bool writing_turn = true;
  const char *path;
  const char *unpackPath;

  std::timespec ts;
  pthread_mutex_t mutex;
  pthread_cond_t cv;

  fmi3_import_t *fmu;
  jm_callbacks callbacks;
  fmi_import_context_t *context;

public:
  FmuNode(const uuid_t &id = {}, const std::string &name = "");

  std::vector<FmuSignal> signalIn;
  std::vector<FmuSignal> signalOut;

  int prepare() override;

  int check() override;

  int start() override;

  int stop() override;

private:
  void parseSignal(json_t *json, bool in);

  double currentTime = 0;
  double step_size = 0.1;
  double stop_time = 0;
  double start_time = 0;
  bool stop_time_defined = false;
  double nextTime = 0.0;

  void getValueReference(const char *varName, fmi3_value_reference_t &ref,
                         fmi3_base_type_enu_t &type);
};

} // namespace node
} // namespace villas
