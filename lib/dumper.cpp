/** Dump fields and values in a socket to plot them with villasDump.py.
 *
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

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <sstream>
#include <cstring>

#include <villas/dumper.hpp>

using namespace villas;
using namespace villas::node;

Dumper::Dumper(const std::string &socketNameIn) :
	socketName(socketNameIn),
	supressRepeatedWarning(true),
	warningCounter(0),
	logger(logging.get("dumper"))
{
	openSocket();
}

Dumper::~Dumper() {
	closeSocket();
}

int Dumper::openSocket()
{
	socketFd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (socketFd < 0) {
		logger->info("Error creating socket {}", socketName);
		return -1;
	}

	sockaddr_un socketaddrUn;
	socketaddrUn.sun_family = AF_UNIX;
	strcpy(socketaddrUn.sun_path, socketName.c_str());

	int ret = connect(socketFd, (struct sockaddr *) &socketaddrUn, sizeof(socketaddrUn));
	if (!ret)
		return ret;

	return 0;
}

int Dumper::closeSocket()
{
	int ret = close(socketFd);
	if (!ret)
		return ret;

	return 0;
}

void Dumper::writeData(unsigned len, double *yData, double *xData)
{
	for (unsigned i = 0; i<len; i++) {
		std::stringstream ss;

		ss << yData[i];

		if(xData != nullptr)
			ss << ";" << xData[i];

		ss << std::endl;

		auto str = ss.str();
		auto bytesWritten =  write(socketFd, str.c_str(), str.length());
		if ((long unsigned int) bytesWritten != str.length() && (!supressRepeatedWarning || warningCounter <1 )) {
			logger->warn("Could not send all content to socket {}", socketName);
			warningCounter++;
		}
	}
}
