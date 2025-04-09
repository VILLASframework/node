/* OpenDSS node-type for electric power distribution system simulator OpenDSS.
 *
 * Author: Jitpanu Maneeratpongsuk <jitpanu.maneeratpongsuk@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/opendss.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static NodeCompatType p;
static NodeCompatFactory ncp(&p);

std::string cmd_command;
char* cmd_result;

static void opendss_parseData(json_t *json, opendss *od, bool in) {
  size_t i;
  json_t *json_data;
  json_error_t err;

  json_array_foreach(json, i, json_data) {
    if (in) {
      int ret;
      const char *name;
      const char *type;
      json_t *a_mode = nullptr;
      Element ele;
      ret = json_unpack_ex(json_data, &err, 0, "{ s: s, s: s, s: o }",
                          "name", &name, "type", &type, "data", &a_mode);
      if (ret)
        throw ConfigError(json, err, "node-config-node-opendss");

      if (!json_is_array(a_mode) && a_mode)
        throw ConfigError(a_mode, "node-config-opendssIn",
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
      json_array_foreach(a_mode, n, json_mode) {
        const char *mode = json_string_value(json_mode);
        // Assign mode according to the OpenDSS function mode
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

      od->dataIn.push_back(ele);
    } else {
      od->monitor_name.push_back(json_string_value(json_data));
    }
  }
}

int villas::node::opendss_parse(NodeCompat *n, json_t *json) {

  int ret;
  auto *od = n->getData<struct opendss>();

  json_error_t err;
  json_t *json_format = nullptr;
  json_t *json_dataIn = nullptr;
  json_t *json_dataOut = nullptr;

  ret = json_unpack_ex(
    json, &err, 0, "{ s?: s, s?: o, s: {s: o}, s: {s: o} }",
    "file_path", &od->path, "format", &json_format, "in", "list", &json_dataIn, "out", "list", &json_dataOut);

  if (ret)
    throw ConfigError(json, err, "node-config-node-opendss");

  if (!json_is_array(json_dataIn) && json_dataIn)
    throw ConfigError(json_dataIn, "node-config-opendssIn",
                      "DataIn must be configured as a list of name objects");

  if (!json_is_array(json_dataOut) && json_dataOut)
    throw ConfigError(json_dataIn, "node-config-opendssIn",
                      "DataOut must be configured as a list of name objects");


  opendss_parseData(json_dataIn, od, true);
  opendss_parseData(json_dataOut, od, false);

  if (od->formatter)
    delete od->formatter;
  od->formatter = json_format ? FormatFactory::make(json_format)
                             : FormatFactory::make("villas.human");
  if (!od->formatter)
    throw ConfigError(json_format, "node-config-node-socket-format",
                      "Invalid format configuration");

  return 0;
}

static std::unordered_set<std::string> opendss_get_element_name(ElementType type) {
  // Get all of the element name for each type and use it to check if the name in the config file is vaild
  std::cout << "opendss get element name" << std::endl;
  uintptr_t myPtr;
	int myType;
	int mySize;
  std::string name;
  std::unordered_set<std::string> set;

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
		if (*(char*)myPtr != '\0') {
			name += *(char*)myPtr;
		} else {
      set.insert(name);
			name = "";
		}
		myPtr++;
	}

  return set;
}

int villas::node::opendss_prepare(NodeCompat *n) {
  auto *od = n->getData<struct opendss>();

  // Compile OpenDSS file
  cmd_command = "compile \"";
  cmd_command.append(od->path);
  cmd_command += "\"";
  cmd_result = DSSPut_Command(cmd_command.data());

  od->load_set = opendss_get_element_name(ElementType::load);
  od->gen_set = opendss_get_element_name(ElementType::generator);
  od->monitor_set = opendss_get_element_name(ElementType::monitor);
  od->isource_set = opendss_get_element_name(ElementType::isource);

  // Check if the name is valid
  for (Element ele: od->dataIn) {
    switch (ele.type) {
      case ElementType::generator:
        if (od->gen_set.find(ele.name) == od->gen_set.end()) {
          throw SystemError("Invalid generator name '{}'", ele.name);
        }
        break;
      case ElementType::load:
        if (od->load_set.find(ele.name) == od->load_set.end()) {
          throw SystemError("Invalid load name '{}'", ele.name);
        }
        break;
      case ElementType::isource:
        if (od->isource_set.find(ele.name) == od->isource_set.end()) {
          throw SystemError("Invalid isource name '{}'", ele.name);
        }
        break;
      default:
        throw SystemError("Invalid input type '{}'", ele.name);
    }
  }

  for (auto m_name: od->monitor_name) {
    if (od->monitor_set.find(m_name) == od->monitor_set.end()) {
      throw SystemError("Invalid monitor name '{}'", m_name);
    }
  }

  return 0;
}

int villas::node::opendss_init(NodeCompat *n) {
  int ret;
  auto *od = n->getData<struct opendss>();
  od->formatter = nullptr;

  ret = pthread_mutex_init(&od->mutex, nullptr);
  if (ret)
    return ret;

  ret = pthread_cond_init(&od->cv, nullptr);
  if (ret)
    return ret;

  return 0;
}

int villas::node::opendss_type_start(SuperNode *sn) {
  int ret;
  // Start OpenDSS
  std::cout << "Node type start" << std::endl;
  ret = DSSI(3,0);

  if (!ret) {
    throw SystemError("Failed to start OpenDSS");
  }

  return 0;
}

int villas::node::opendss_start(NodeCompat *n) {
  auto *od = n->getData<struct opendss>();
  od->formatter->start(n->getInputSignals(false), ~(int)SampleFlags::HAS_OFFSET);
  od->writing_turn = true; // Start with opendss_write
  return 0;
}

static int opendss_extractMonitorData(opendss *od, struct Sample *const *smps) {
  // Get the data from the OpenDSS monitor
  uintptr_t myPtr;
  int myType;
  int mySize;
  int data_count = 0;
  for (auto& Name:od->monitor_name) {
    MonitorsS(2, Name.data());
    MonitorsV(1, &myPtr, &myType, &mySize);

    int channel = MonitorsI(17, 0);
    if (data_count == 0) {
      channel += 2;
    }

    float* data_ptr = reinterpret_cast<float*>(myPtr);
    data_ptr += (mySize / 4) - channel;
    for (int i = 0; i < channel; i++) {
      smps[0]->data[data_count + i].f = *data_ptr;
      data_ptr++;
    }
    data_count += channel;
  }
  return data_count;
}

int villas::node::opendss_read(NodeCompat *n, struct Sample *const *smps, unsigned int cnt) {

  if (n->getState() != State::STARTED) {
    return -1;
  }

  int ret = 0;
  auto *od = n->getData<struct opendss>();

  // Wait until writing is done
  pthread_mutex_lock(&od->mutex);
  while (od->writing_turn) {
    pthread_cond_wait(&od->cv, &od->mutex);
  }

  smps[0]->ts.origin = od->ts;
  smps[0]->flags = (int)SampleFlags::HAS_DATA | (int)SampleFlags::HAS_TS_ORIGIN | (int)SampleFlags::HAS_SEQUENCE;
  smps[0]->signals = n->getInputSignals(false);
  smps[0]->length = 0;

  // Solve OpenDSS file
  SolutionI(0, 0);

  smps[0]->length = opendss_extractMonitorData(od, smps);
  smps[0]->sequence = smps[0]->data[0].f;

  ret = 1;
  od->writing_turn = true;
  pthread_cond_signal(&od->cv);
  pthread_mutex_unlock(&od->mutex);

  return ret;

}

int villas::node::opendss_write(NodeCompat *n, struct Sample *const *smps, unsigned int cnt) {

  auto *od = n->getData<struct opendss>();

  // Wait until reading is done
  pthread_mutex_lock(&od->mutex);
  while (!od->writing_turn) {
    pthread_cond_wait(&od->cv, &od->mutex);
  }

  od->ts = smps[0]->ts.origin;
  int i = 0;

  for (auto &ele:od->dataIn) {
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

    for (auto &m: ele.mode) {
      func(m, smps[0]->data[i].f);
      i++;
    }
  }

  od->writing_turn = false;
  pthread_cond_signal(&od->cv);
  pthread_mutex_unlock(&od->mutex);

  return cnt;
}

int villas::node::opendss_stop(NodeCompat *n) {

  // Close OpenDSS
  cmd_command = "CloseDI";
	cmd_result = DSSPut_Command(cmd_command.data());

	return 0;
}

int villas::node::opendss_destroy(NodeCompat *n) {
  int ret;
  auto *od = n->getData<struct opendss>();

  if (od->formatter)
    delete od->formatter;

  ret = pthread_mutex_destroy(&od->mutex);
  if (ret)
    return ret;

  ret = pthread_cond_destroy(&od->cv);
  if (ret)
    return ret;

  return 0;
}


__attribute__((constructor(110))) static void register_plugin() {
  p.name = "opendss";
  p.description = "OpenDSS simulator node";
  p.vectorize = 0;
  p.size = sizeof(struct opendss);
  p.type.start = opendss_type_start;
  p.init = opendss_init;
  p.destroy = opendss_destroy;
  p.prepare = opendss_prepare;
  p.parse = opendss_parse;
  p.start = opendss_start;
  p.stop = opendss_stop;
  p.read = opendss_read;
  p.write = opendss_write;
}
