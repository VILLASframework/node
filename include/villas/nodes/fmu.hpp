/* Node type: Functional Mock-up unit.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <vector>

#include <fmilib.h>

#include <villas/format.hpp>
#include <villas/node.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class FmuNode : public Node {
protected:
  int parse(json_t *json) override;

  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;

  bool writingTurn = true;
  const char *path;
  const char *unpack_path;

  timespec ts;
  pthread_mutex_t mutex;
  pthread_cond_t cv;

  fmi3_import_t *fmu;
  jm_callbacks callbacks;
  fmi_import_context_t *context;

public:
  FmuNode(const uuid_t &id = {}, const std::string &name = "");
  struct fmu_signal {
    unsigned int ref = 0;
    fmi3_base_type_enu_t type = fmi3_base_type_enu_t::fmi3_base_type_float64;
    std::string name;
  };

  std::vector<fmu_signal> signalIn;
  std::vector<fmu_signal> signalOut;

  int prepare() override;

  int check() override;

  int start() override;

  int stop() override;

  // ~FmuNode();

private:
  void parse_signal(json_t *json, bool in);
  double currentTime = 0;
  double stepSize = 0.1;
  double stopTime = 0;
  double startTime = 0;
  bool stopTimeDef = false;
  double nextTime = 0.0;
  void get_vr(const char *var_name, fmi3_value_reference_t &ref,
              fmi3_base_type_enu_t &type);
};

} // namespace node
} // namespace villas
