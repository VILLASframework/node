/** Node type: IEC60870-5-104
 *
 * @file
 * @author Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <ctime>
#include <array>
#include <villas/node/config.hpp>
#include <villas/node.hpp>
#include <villas/pool.hpp>
#include <villas/signal.hpp>
#include <lib60870/cs104_connection.h>

namespace villas {
namespace node {
namespace iec60870_client {

class IEC60870_client : public Node {
protected:
	struct Server {
		// Config (use explicit defaults)
		std::string address;
		int port;
		int common_address;

		// Lib60870
		CS104_Connection conHandle;

	} server;

	virtual
	int _write(struct Sample *smps[], unsigned cnt) override;

	virtual
	int _read(struct Sample * smps[], unsigned cnt) override;

public:
	IEC60870_client(const std::string &name = "");

	virtual
	~IEC60870_client() override;

	virtual
	int parse(json_t *json, const uuid_t sn_uuid) override;

	virtual
	int start() override;

	virtual
	int stop() override;
};

} /* namespace iec60870 */
} /* namespace node */
} /* namespace villas */
