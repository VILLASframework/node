/* OpenDSS node-type for electric power distribution system simulator OpenDSS.
 *
 * Author: Jitpanu Maneeratpongsuk <jitpanu.maneeratpongsuk@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <unordered_set>

#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/timing.hpp>
#include <villas/format.hpp>
#include <villas/node_compat.hpp>

extern "C" int DSSI(int mode, int arg);
extern "C" char* DSSPut_Command(char* myCmd);
extern "C" int SolutionI(int Parameter, int arg);
extern "C" char* MonitorsS(int mode, char* arg);

extern "C" char* DSSLoadsS(int mode, char* arg);
extern "C" double DSSLoadsF(int mode, double arg);
extern "C" void DSSLoadsV(int mode, uintptr_t* myPtr, int* myType, int* mySize);

extern "C" char* GeneratorsS(int mode, char* arg);
extern "C" double GeneratorsF (int mode, double arg);
extern "C" void GeneratorsV(int mode, uintptr_t* myPtr, int* myType, int* mySize);

extern "C" char* IsourceS(int mode, char* arg);
extern "C" double IsourceF (int mode, double arg);
extern "C" void IsourceV(int mode, uintptr_t* myPtr, int* myType, int* mySize);

extern "C" char* MonitorsS(int mode, char* arg);
extern "C" void MonitorsV(int mode, uintptr_t *myPtr, int *myType, int *mySize);
extern "C" int MonitorsI(int mode, int arg);

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class OpenDSS : public Node {
protected:
  enum ElementType {
    generator,
    load,
    monitor,
    isource
  };

  struct Element {
    std::string name;      // Name of the element must be that same as declared in OpenDSS file
    ElementType type;      // Type of the element
    std::vector<int> mode; // Mode is set according to the function mode in OpenDSS
  };

  virtual int _read(struct Sample *smps[], unsigned cnt);

  virtual int _write(struct Sample *smps[], unsigned cnt);

  void parseData(json_t *json, bool in);

  void getElementName(ElementType type, std::unordered_set<std::string> *set);

  int extractMonitorData(struct Sample *const *smps);

public:
  OpenDSS(const uuid_t &id = {}, const std::string &name = "");

  bool writing_turn;
  const char *path;
  timespec ts;

  std::string cmd_command;
  char* cmd_result;

  std::vector<Element> dataIn;  // Vector of element to be written
  std::vector<std::string> monitor_name;
  std::unordered_set<std::string> load_set, gen_set, monitor_set, isource_set;  // Set of corresponding element type

  pthread_mutex_t mutex;
  pthread_cond_t cv;

  virtual ~OpenDSS();

  virtual int prepare();

  virtual int parse(json_t *json);

  virtual int check();

  virtual int start();

  virtual int stop();
};

} // namespace node
} // namespace villas
