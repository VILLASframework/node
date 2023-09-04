/* Key generator for libzmq.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>
#include <cstdlib>
#include <cassert>
#include <zmq.h>
#include <villas/tool.hpp>

#if ZMQ_VERSION_MAJOR < 4 || (ZMQ_VERSION_MAJOR == 4 && ZMQ_VERSION_MINOR <= 1)
  #include <zmq_utils.h>
#endif

namespace villas {
namespace node {
namespace tools {

class ZmqKeygen : public Tool {

public:
	ZmqKeygen(int argc, char *argv[]) :
		Tool(argc, argv, "zmq-keygen")
	{ }

protected:
	int main()
	{
		int ret;
		char public_key[41];
		char secret_key[41];

		ret = zmq_curve_keypair(public_key, secret_key);
		if (ret) {
			if (zmq_errno() == ENOTSUP)
				std::cout << "To use " << argv[0] << ", please install libsodium and then rebuild libzmq." << std::endl;

			exit(EXIT_FAILURE);
		}

		std::cout << "# Copy these lines to your 'zeromq' node-configuration" << std::endl;
		std::cout << "curve = {" << std::endl;
		std::cout << "\tpublic_key = \"" << public_key << "\";" << std::endl;
		std::cout << "\tsecret_key = \"" << secret_key << "\";" << std::endl;
		std::cout << "}" << std::endl;

		return 0;
	}
};

} // namespace tools
} // namespace node
} // namespace villas

int main(int argc, char *argv[])
{
	villas::node::tools::ZmqKeygen t(argc, argv);

	return t.run();
}
