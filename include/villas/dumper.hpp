/** Dump fields and values in a socket to plot them with villasDump.py.
 *
 * @file
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#pragma once

#include <villas/log.hpp>

#include <string>

namespace villas {
namespace node {

class Dumper {

protected:
	int socketFd;
	std::string socketName;
	bool supressRepeatedWarning;
	uint64_t warningCounter;
	Logger logger;

public:
	Dumper(const std::string &socketNameIn);
	~Dumper();

	int openSocket();
	int closeSocket();
	

	void writeDataCSV(unsigned len, double *yData, double *xData = nullptr);
	void writeDataBinary(unsigned len, double *yData, double *xData = nullptr);
};

} /* namespace node */
} /* namespace villas */
