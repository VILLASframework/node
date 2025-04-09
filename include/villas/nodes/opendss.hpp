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
#include "villas/format.hpp"
#include "villas/node_compat.hpp"

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

enum ElementType {
  generator,
  load,
  monitor,
  isource
};

struct Element {
  std::string name;      // Name of the element must be that same as declared in OpenDSS file
  ElementType type;      // Type of the element
  std::vector<int> mode; // Mode is set according to the function mode in opendss
};

struct opendss {
  Format *formatter;
  const char *path;
  std::vector<Element> dataIn;  // Vector of element to be written
  std::vector<std::string> monitor_name;
  std::unordered_set<std::string> load_set, gen_set, monitor_set, isource_set;  // Set of corresponding element type
  bool writing_turn;
  timespec ts;
  pthread_mutex_t mutex;
  pthread_cond_t cv;

};

int opendss_parse(NodeCompat *n, json_t *json);

int opendss_prepare(NodeCompat *n);

int opendss_init(NodeCompat *n);

int opendss_start(NodeCompat *n);

int opendss_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int opendss_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int opendss_type_start(SuperNode *sn);

int opendss_stop(NodeCompat *n);

int opendss_destroy(NodeCompat *n);

} // namespace node
} // namespace villas
