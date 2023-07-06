/** Dump fields and values in a socket to plot them with villasDump.py.
 *
 * @file
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <villas/log.hpp>

#include <string>

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

} /* namespace node */
} /* namespace villas */
