/** Dump fields and values in a socket to plot them with villasDump.py.
 *
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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

Dumper::Dumper() :
	active(false),
	socketFd(0),
	socketPath(""),
	supressRepeatedWarning(true),
	warningCounter(0),
	logger(logging.get("dumper"))
{}

Dumper::~Dumper() {
	closeSocket();
}

bool Dumper::isActive()
{
	return active;
}

int Dumper::setActive()
{
	active = true;
	return 1;
}

int Dumper::openSocket()
{
	socketFd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (socketFd < 0) {
		logger->info("Error creating socket {}", socketPath);
		return -1;
	}

	sockaddr_un socketaddrUn;
	socketaddrUn.sun_family = AF_UNIX;
	strcpy(socketaddrUn.sun_path, socketPath.c_str());

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

int Dumper::setPath(std::string socketPathIn)
{
	socketPath = socketPathIn;
	return 1;
}

void Dumper::writeDataBinary(unsigned len, double *yData, double *xData)
{

	if (warningCounter > 10)
		return;

	if (yData == nullptr)
		return;

	unsigned dataLen = len * sizeof(double);
	ssize_t bytesWritten = write(socketFd, &dataLen, sizeof(dataLen));
	if ((size_t) bytesWritten != sizeof(len)) {
		logger->warn("Could not send all content (Len) to socket {}", socketPath);
		warningCounter++;
	}

	static const char buf[] = "d000";
	bytesWritten = write(socketFd, buf, sizeof(buf));
	if (bytesWritten != sizeof(buf)) {
		logger->warn("Could not send all content (Type) to socket {}", socketPath);
		warningCounter++;
	}

	bytesWritten = write(socketFd, yData, dataLen );
	if (bytesWritten != (ssize_t) dataLen && (!supressRepeatedWarning || warningCounter <1 )) {
		logger->warn("Could not send all content (Data) to socket {}", socketPath);
		warningCounter++;
	}
}

void Dumper::writeDataCSV(unsigned len, double *yData, double *xData)
{
	for (unsigned i = 0; i < len; i++) {
		std::stringstream ss;

		ss << yData[i];

		if (xData != nullptr)
			ss << ";" << xData[i];

		ss << std::endl;

		auto str = ss.str();
		auto bytesWritten =  write(socketFd, str.c_str(), str.length());
		if ((size_t) bytesWritten != str.length() && (!supressRepeatedWarning || warningCounter < 1)) {
			logger->warn("Could not send all content to socket {}", socketPath);
			warningCounter++;
		}
	}
}
