/* Node type: Functional Mock-up Unit.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>, Jitpanu Maneeratpongsuk <jitpanu.maneeratpongsuk@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cerrno>
#include <cstddef>
#include <ctime>

#include <FMI3/fmi3_function_types.h>
#include <FMI3/fmi3_types.h>
#include <JM/jm_callbacks.h>
#include <JM/jm_types.h>
#include <fmilib.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/node.hpp>
#include <villas/nodes/fmu.hpp>
#include <villas/timing.hpp>

using namespace villas;
using namespace villas::node;

FmuNode::FmuNode(const uuid_t &id, const std::string &name)
    : Node(id, name), writing_turn(false), path(""), unpackPath(""), fmu(),
      context() {
  int ret;
  ret = pthread_mutex_init(&mutex, nullptr);
  if (ret)
    throw RuntimeError("Failed to initialize mutex");
  ret = pthread_cond_init(&cv, nullptr);
  if (ret)
    throw RuntimeError("Failed to initialize conditional variable");

  // Initialize callbacks
  callbacks.malloc = malloc;
  callbacks.calloc = calloc;
  callbacks.realloc = realloc;
  callbacks.free = free;
  callbacks.logger = nullptr;
  callbacks.log_level = jm_log_level_info;
  callbacks.context = nullptr;

  // Allocate FMI context
  context = fmi_import_allocate_context(&callbacks);
}

int FmuNode::prepare() {
  callbacks.malloc = malloc;
  callbacks.calloc = calloc;
  callbacks.realloc = realloc;
  callbacks.free = free;
  callbacks.logger = nullptr;
  callbacks.log_level = jm_log_level_info;
  callbacks.context = nullptr;
  context = fmi_import_allocate_context(&callbacks);

  struct stat sb;
  if (stat(unpackPath, &sb) != 0)
    logger->error("The unpack path is invalid: {}, {}", strerror(errno),
                  unpackPath);

  fmi_version_enu_t version =
      fmi_import_get_fmi_version(context, path, unpackPath);
  if (version != fmi_version_enu_t::fmi_version_3_0_enu) {
    throw RuntimeError("Not FMI 3.0");
  }

  fmu = fmi3_import_parse_xml(context, unpackPath, 0);
  if (!fmu) {
    throw RuntimeError("Failed to parse xml file");
  }

  if (fmi3_import_create_dllfmu(fmu, fmi3_fmu_kind_cs, NULL, NULL) !=
      jm_status_success) {
    throw RuntimeError("Failed to load FMU binary");
  }

  if (fmi3_import_instantiate_co_simulation(
          fmu, "instance1", unpackPath, fmi3_false, fmi3_true, fmi3_false,
          fmi3_true, NULL, 0, NULL) != jm_status_success) {
    throw RuntimeError("Failed to instantiate FMU");
  }

  fmi3_import_enter_initialization_mode(fmu, fmi3_false, 0.0, start_time,
                                        stop_time_defined, stop_time);
  fmi3_import_exit_initialization_mode(fmu);

  // Fetch ref and type of input and output FMU variables
  for (size_t i = 0; i < signalIn.size(); i++) {
    getValueReference(signalIn[i].name.data(), signalIn[i].ref,
                      signalIn[i].type);
  }
  for (size_t i = 0; i < signalOut.size(); i++) {
    getValueReference(signalOut[i].name.data(), signalOut[i].ref,
                      signalOut[i].type);
  }

  return Node::prepare();
}

int FmuNode::check() {
  return Node::check();
}

int FmuNode::_read(struct Sample *smps[], unsigned cnt) {
  assert(cnt == 1);

  const bool hasInputs = !signalIn.empty();
  // Lock thread
  pthread_mutex_lock(&mutex);
  while (hasInputs && (writing_turn || currentTime >= stop_time)) {
    pthread_cond_wait(&cv, &mutex);
  }
  double targetTime = currentTime + step_size;
  double remainingStep = step_size;
  smps[0]->signals = out.signals;

  while (remainingStep > 0.0) {
    bool eventHandlingNeeded = false;
    bool terminateSimulation = false;
    bool earlyReturn = false;
    double lastSuccessfulTime = currentTime;

    // Perform step
    fmi3_status_t status = fmi3_import_do_step(
        fmu, currentTime, step_size, fmi3_true, &eventHandlingNeeded,
        &terminateSimulation, &earlyReturn, &lastSuccessfulTime);

    if (status == fmi3_status_error)
      throw RuntimeError("Error during step");

    if (terminateSimulation) {
      logger->info("FMU called for simulation termination");
      currentTime = lastSuccessfulTime;
      break;
    }

    if (earlyReturn) {
      logger->info("FMU returned early at {}, target time {}",
                   lastSuccessfulTime, targetTime);
      currentTime = lastSuccessfulTime;
      remainingStep = targetTime - currentTime;
    } else {
      currentTime += remainingStep;
      remainingStep = 0.0;
    }
  }

  smps[0]->ts.origin = time_now();
  smps[0]->flags = (int)SampleFlags::HAS_DATA | (int)SampleFlags::HAS_TS_ORIGIN;
  smps[0]->length = signalOut.size();
  double val = 0;
  for (size_t i = 0; i < signalOut.size(); i++) {
    switch (signalOut[i].type) {
      using enum fmi3_base_type_enu_t;
    case fmi3_base_type_float64:
      fmi3_import_get_float64(fmu, &signalOut[i].ref, 1, &val, 1);
      break;
    case fmi3_base_type_float32: {
      float data = 0;
      fmi3_import_get_float32(fmu, &signalOut[i].ref, 1, &data, 1);
      val = data;
      break;
    }
    case fmi3_base_type_int64: {
      long data = 0;
      fmi3_import_get_int64(fmu, &signalOut[i].ref, 1, &data, 1);
      val = data;
      break;
    }
    case fmi3_base_type_int32: {
      int data = 0;
      fmi3_import_get_int32(fmu, &signalOut[i].ref, 1, &data, 1);
      val = data;
      break;
    }
    case fmi3_base_type_bool: {
      bool data = 0;
      fmi3_import_get_boolean(fmu, &signalOut[i].ref, 1, &data, 1);
      val = data;
      break;
    }
    default:
      throw RuntimeError("Unsupported data type");
      break;
    }
    auto sig = smps[0]->signals->getByIndex(i);
    switch (sig->type) {
    case villas::node::SignalType::FLOAT:
      smps[0]->data[i].f = val;
      break;
    case villas::node::SignalType::INTEGER:
      smps[0]->data[i].i = val;
      break;
    case villas::node::SignalType::BOOLEAN:
      smps[0]->data[i].b = val;
      break;
    default:
      break;
    }
  }

  // Unlock thread
  if (hasInputs) {
    writing_turn = true;
    pthread_cond_signal(&cv);
  }

  pthread_mutex_unlock(&mutex);

  return 1;
}

int FmuNode::_write(struct Sample *smps[], unsigned cnt) {
  assert(cnt == 1);
  // Lock thread
  if (signalIn.empty())
    return 0;
  pthread_mutex_lock(&mutex);
  while (!writing_turn) {
    pthread_cond_wait(&cv, &mutex);
  }
  ts = smps[0]->ts.origin;
  for (size_t i = 0; i < signalIn.size(); i++) {
    auto sig = smps[0]->signals->getByIndex(i);
    double val = 0;
    switch (sig->type) {
    case villas::node::SignalType::FLOAT:
      val = smps[0]->data[i].f;
      break;
    case villas::node::SignalType::INTEGER:
      val = smps[0]->data[i].i;
      break;
    case villas::node::SignalType::BOOLEAN:
      val = smps[0]->data[i].b;
      break;
    default:
      break;
    }
    logger->debug("Input value {}", val);
    switch (signalIn[i].type) {
      using enum fmi3_base_type_enu_t;
    case fmi3_base_type_float64:
      fmi3_import_set_float64(fmu, &signalIn[i].ref, 1, &val, 1);
      break;
    case fmi3_base_type_float32: {
      float data = val;
      fmi3_import_set_float32(fmu, &signalIn[i].ref, 1, &data, 1);
      break;
    }
    case fmi3_base_type_int64: {
      long data = floor(val);
      fmi3_import_set_int64(fmu, &signalIn[i].ref, 1, &data, 1);
      break;
    }
    case fmi3_base_type_int32: {
      int data = floor(val);
      fmi3_import_set_int32(fmu, &signalIn[i].ref, 1, &data, 1);
      break;
    }
    case fmi3_base_type_bool: {
      bool data = val;
      fmi3_import_set_boolean(fmu, &signalIn[i].ref, 1, &data, 1);
      break;
    }
    default:
      throw RuntimeError("Unsupported data type");
      break;
    }
  }

  // Unlock thread
  writing_turn = false;
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&mutex);

  return 1;
}

void FmuNode::getValueReference(const char *varName,
                                fmi3_value_reference_t &ref,
                                fmi3_base_type_enu_t &type) {
  fmi3_import_variable_t *var = fmi3_import_get_variable_by_name(fmu, varName);
  if (!var) {
    throw SystemError("Invalid variable name '{}'", varName);
  }
  type = fmi3_import_get_variable_base_type(var);
  ref = fmi3_import_get_variable_vr(var);
}

int FmuNode::parse(json_t *json) {
  int ret = Node::parse(json);
  if (ret)
    return ret;

  json_t *json_signals_in = nullptr;
  json_t *json_signals_out = nullptr;
  json_error_t err;
  start_time = 0;
  step_size = 0.1;
  stop_time = INT_MAX;
  ret = json_unpack_ex(
      json, &err, 0, "{s:s, s:s, s?:f, s?:f, s?:f, s?:{s:o}, s?:{s:o}}",
      "fmu_path", &path, "fmu_unpack_path", &unpackPath, "stop_time",
      &stop_time, "start_time", &start_time, "step_size", &step_size, "in",
      "signals", &json_signals_in, "out", "signals", &json_signals_out);
  if (ret)
    throw ConfigError(json, err, "node-config-node-fmu");
  if (stop_time == INT_MAX) {
    stop_time_defined = false;
  } else {
    stop_time_defined = true;
  }

  if (in.signals) {
    for (const auto &sig : *in.signals) {
      FmuSignal s;
      s.name = sig->name;
      signalIn.push_back(s);
    }
  }

  // Initially set writing_turn to false if there are no input signals, _read() calls do_step() with default inputs.
  writing_turn = !signalIn.empty();

  if (out.signals) {
    for (const auto &sig : *out.signals) {
      FmuSignal s;
      s.name = sig->name;
      signalOut.push_back(s);
    }
  }

  return 0;
}

int FmuNode::start() {
  return Node::start();
}

int FmuNode::stop() {
  fmi3_import_terminate(fmu);
  fmi3_import_free_instance(fmu);
  fmi3_import_destroy_dllfmu(fmu);
  fmi3_import_free(fmu);
  fmi_import_free_context(context);
  return 0;
}

// Register node
static char n[] = "fmu";
static char d[] = "Interface to Functional Mockup Unit using FMI3.0";
static NodePlugin<FmuNode, n, d,
                  (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_WRITE>
    p;
