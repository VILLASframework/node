/* OpenDSS node-type for electric power distribution system simulator OpenDSS.
 *
 * Author: Jitpanu Maneeratpongsuk <jitpanu.maneeratpongsuk@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

// clang-format off
// OpenDSS has broken header files that conflict with std::complex
#include <OpenDSSCDLL.h>
// clang-format on

#include <fmt/core.h>

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/opendss.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

OpenDSS::OpenDSS(const uuid_t &id, const std::string &name)
    : Node(id, name), writingTurn(false), path("") {
  int ret = pthread_mutex_init(&mutex, nullptr);
  if (ret)
    throw RuntimeError("failed to initialize mutex");

  ret = pthread_cond_init(&cv, nullptr);
  if (ret)
    throw RuntimeError("failed to initialize mutex");
}

OpenDSS::~OpenDSS() {
  int ret __attribute__((unused));

  ret = pthread_mutex_destroy(&mutex);
  ret = pthread_cond_destroy(&cv);
}

std::string OpenDSS::dssPutCommand(const std::string &cmd) const {
  static std::string cmdCommand = cmd;

  return DSSPut_Command(cmdCommand.data());
}

void OpenDSS::parseData(json_t *json, bool in) {
  size_t i;
  json_t *json_data;
  json_error_t err;

  json_array_foreach (json, i, json_data) {
    if (in) {
      const char *name;
      const char *type;
      json_t *a_mode = nullptr;
      Element ele;

      int ret = json_unpack_ex(json_data, &err, 0, "{ s: s, s: s, s: o }",
                               "name", &name, "type", &type, "data", &a_mode);
      if (ret)
        throw ConfigError(json, err, "node-config-node-opendss");

      if (!json_is_array(a_mode) && a_mode)
        throw ConfigError(
            a_mode, "node-config-opendssIn",
            "datatype must be configured as a list of name objects");

      ele.name = name;

      if (!strcmp(type, "generator"))
        ele.type = ElementType::generator;
      else if (!strcmp(type, "load"))
        ele.type = ElementType::load;
      else if (!strcmp(type, "isource"))
        ele.type = ElementType::isource;
      else
        throw SystemError("Invalid element type '{}'", type);

      size_t n;
      json_t *json_mode;
      json_array_foreach (a_mode, n, json_mode) {
        const char *mode = json_string_value(json_mode);
        // Assign mode according to the OpenDSS function mode.
        switch (ele.type) {
        case ElementType::generator:
          if (!strcmp(mode, "kV"))
            ele.mode.push_back(1);
          else if (!strcmp(mode, "kW"))
            ele.mode.push_back(3);
          else if (!strcmp(mode, "kVar"))
            ele.mode.push_back(5);
          else if (!strcmp(mode, "Pf"))
            ele.mode.push_back(7);
          else
            throw SystemError("Invalid data type: {}", mode);
          break;
        case ElementType::load:
          if (!strcmp(mode, "kW"))
            ele.mode.push_back(1);
          else if (!strcmp(mode, "kV"))
            ele.mode.push_back(3);
          else if (!strcmp(mode, "kVar"))
            ele.mode.push_back(5);
          else if (!strcmp(mode, "Pf"))
            ele.mode.push_back(7);
          else
            throw SystemError("Invalid data type: {}", mode);
          break;
        case ElementType::isource:
          if (!strcmp(mode, "Amps"))
            ele.mode.push_back(1);
          else if (!strcmp(mode, "AngleDeg"))
            ele.mode.push_back(3);
          else if (!strcmp(mode, "Frequency"))
            ele.mode.push_back(5);
          else
            throw SystemError("Invalid data type: {}", mode);
          break;
        default:
          throw SystemError("Invalid element type");
          break;
        }
      }
      dataIn.push_back(ele);
    } else {
      monitorNames.push_back(json_string_value(json_data));
    }
  }
}

int OpenDSS::parse(json_t *json) {

  int ret = Node::parse(json);
  if (ret)
    return ret;

  json_error_t err;
  json_t *json_dataIn = nullptr;
  json_t *json_dataOut = nullptr;

  ret = json_unpack_ex(json, &err, 0, "{ s?: s, s: {s: o}, s: {s: o} }",
                       "file_path", &path, "in", "list", &json_dataIn, "out",
                       "list", &json_dataOut);

  if (ret)
    throw ConfigError(json, err, "node-config-node-opendss");

  if (!json_is_array(json_dataIn) && json_dataIn)
    throw ConfigError(json_dataIn, "node-config-opendssIn",
                      "DataIn must be configured as a list of name objects");

  if (!json_is_array(json_dataOut) && json_dataOut)
    throw ConfigError(json_dataIn, "node-config-opendssIn",
                      "DataOut must be configured as a list of name objects");

  parseData(json_dataIn, true);
  parseData(json_dataOut, false);

  return 0;
}

void OpenDSS::getElementName(ElementType type,
                             std::unordered_set<std::string> *set) {
  // Get all of the element name for each type and use it to check if the name in the config file is vaild.
  uintptr_t myPtr;
  int myType;
  int mySize = 0;
  std::string name;

  switch (type) {
  case ElementType::generator:
    GeneratorsV(0, &myPtr, &myType, &mySize);
    break;
  case ElementType::load:
    DSSLoadsV(0, &myPtr, &myType, &mySize);
    break;
  case ElementType::monitor:
    MonitorsV(0, &myPtr, &myType, &mySize);
    break;
  case ElementType::isource:
    IsourceV(0, &myPtr, &myType, &mySize);
    break;
  }

  for (int i = 0; i < mySize; i++) {
    if (*(char *)myPtr != '\0') {
      name += *(char *)myPtr;
    } else {
      set->insert(name);
      name = "";
    }
    myPtr++;
  }
}

int OpenDSS::prepare() {
  // Start OpenDSS.
  int ret = DSSI(3, 0);
  if (!ret) {
    throw SystemError("Failed to start OpenDSS");
  }

  // Hide OpenDSS terminal output.
  DSSI(8, 0);

  // Compile OpenDSS file.
  dssPutCommand(fmt::format("compile \"{}\"", path));

  getElementName(ElementType::load, &loads);
  getElementName(ElementType::generator, &generators);
  getElementName(ElementType::monitor, &monitors);
  getElementName(ElementType::isource, &isource_set);

  // Check if element name is valid
  for (Element ele : dataIn) {
    switch (ele.type) {
    case ElementType::generator:
      if (generators.find(ele.name) == generators.end()) {
        throw SystemError("Invalid generator name '{}'", ele.name);
      }
      break;
    case ElementType::load:
      if (loads.find(ele.name) == loads.end()) {
        throw SystemError("Invalid load name '{}'", ele.name);
      }
      break;
    case ElementType::isource:
      if (isource_set.find(ele.name) == isource_set.end()) {
        throw SystemError("Invalid isource name '{}'", ele.name);
      }
      break;
    default:
      throw SystemError("Invalid input type '{}'", ele.name);
    }
  }

  for (auto m_name : monitorNames) {
    if (monitors.find(m_name) == monitors.end()) {
      throw SystemError("Invalid monitor name '{}'", m_name);
    }
  }

  return Node::prepare();
}

int OpenDSS::start() {
  // Start with writing.
  writingTurn = true;

  return Node::start();
}

int OpenDSS::extractMonitorData(struct Sample *const *smps) {
  // Get the data from the OpenDSS monitor.
  uintptr_t myPtr;
  int myType;
  int mySize;
  int data_count = 0;

  for (auto &Name : monitorNames) {
    MonitorsS(2, Name.data());
    MonitorsV(1, &myPtr, &myType, &mySize);

    int channel = MonitorsI(17, 0);
    if (data_count == 0) {
      channel += 2;
    }

    float *data_ptr = reinterpret_cast<float *>(myPtr);
    data_ptr += (mySize / 4) - channel;
    for (int i = 0; i < channel; i++) {
      smps[0]->data[data_count + i].f = *data_ptr;
      data_ptr++;
    }
    data_count += channel;
  }

  return data_count;
}

int OpenDSS::_read(struct Sample *smps[], unsigned cnt) {
  // Wait until writing is done.
  pthread_mutex_lock(&mutex);
  while (writingTurn) {
    pthread_cond_wait(&cv, &mutex);
  }

  smps[0]->ts.origin = ts;
  smps[0]->flags = (int)SampleFlags::HAS_DATA |
                   (int)SampleFlags::HAS_TS_ORIGIN |
                   (int)SampleFlags::HAS_SEQUENCE;
  smps[0]->length = 0;

  // Solve OpenDSS file.
  SolutionI(0, 0);

  smps[0]->length = extractMonitorData(smps);
  smps[0]->sequence = smps[0]->data[0].f;

  writingTurn = true;
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&mutex);

  return 1;
}

int OpenDSS::_write(struct Sample *smps[], unsigned cnt) {
  // Wait until reading is done.
  pthread_mutex_lock(&mutex);
  while (!writingTurn) {
    pthread_cond_wait(&cv, &mutex);
  }

  ts = smps[0]->ts.origin;

  int i = 0;
  for (auto &ele : dataIn) {
    double (*func)(int, double);
    switch (ele.type) {
    case ElementType::generator:
      func = GeneratorsF;
      GeneratorsS(1, ele.name.data());
      break;
    case ElementType::load:
      func = DSSLoadsF;
      DSSLoadsS(1, ele.name.data());
      break;
    case ElementType::isource:
      func = IsourceF;
      IsourceS(1, ele.name.data());
      break;
    default:
      throw SystemError("Invalid element type");
    }

    for (auto &m : ele.mode) {
      func(m, smps[0]->data[i].f);
      i++;
    }
  }

  writingTurn = false;
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&mutex);

  return cnt;
}

int OpenDSS::stop() {
  dssPutCommand("CloseDI"); // Close OpenDSS.

  return Node::stop();
}

// Register node.
static char n[] = "opendss";
static char d[] = "Interface to OpenDSS, EPRI's Distribution System Simulator";
static NodePlugin<OpenDSS, n, d,
                  (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_WRITE,
                  1>
    p;
