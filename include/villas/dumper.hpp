/* Dump fields and values in a socket to plot them with villasDump.py.
 *
 * Author: Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>

#include <villas/log.hpp>

namespace villas {
namespace node {

class Dumper {

protected:
  bool active;
  int socketFd;
  std::string socketPath;
  bool supressRepeatedWarning;
  uint64_t warningCounter;
  Logger logger;

public:
  Dumper();
  ~Dumper();

  int openSocket();
  int closeSocket();
  bool isActive();
  int setActive();

  int setPath(const std::string &socketPathIn);
  void writeDataCSV(unsigned len, double *yData, double *xData = nullptr);
  void writeDataBinary(unsigned len, double *yData, double *xData = nullptr);
};

} // namespace node
} // namespace villas
