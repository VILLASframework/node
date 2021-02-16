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

#include <villas/dumper.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <villas/log.h>
#include <unistd.h>

using namespace villas;
using namespace villas::node;

Dumper::Dumper(std::string socketNameIn) :
	socketName(""),
	supressRepeatedWarning(true),
	warningCounter(0)
{
	openSocket(socketNameIn);
}

Dumper::~Dumper() {
	closeSocket();
}

int Dumper::openSocket(std::string socketNameIn)
{
	socketName = socketNameIn;

	socketFd = socket(AF_LOCAL,SOCK_STREAM, 0);
	if (socketFd < 0) {
		info("Error creating socket %s", socketName.c_str());
		return -1;
	}

	sockaddr_un socketaddrUn;
	socketaddrUn.sun_family = AF_UNIX;
	strcpy(socketaddrUn.sun_path, socketName.c_str());
	socketName = socketNameIn;

	connect(socketFd, (struct sockaddr *) &socketaddrUn, sizeof(socketaddrUn));

	return 1;
}

void Dumper::closeSocket()
{
	info("Remove socket");
	close(socketFd);
}

void Dumper::writeData(uint len, double *yData, double *xData)
{
	ssize_t bytesWritten;

	for (uint i = 0; i<len; i++) {
		std::string str = std::to_string(yData[i]);
		if(xData != nullptr)
			str+= ";" + std::to_string(xData[i]);
		str += "\n";

		bytesWritten =  write(socketFd, str.c_str(), str.length());
		if ((long unsigned int) bytesWritten != str.length() && (!supressRepeatedWarning ||  warningCounter<1)) {
			warning("Could not send all content to socket %s", socketName.c_str());
			warningCounter++;
		}
	}
}
