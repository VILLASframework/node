/**
 * @file
 * @author Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <string>
#include <optional>
#include <villas/nodes/c37_118/parser.hpp>
#include <villas/node/config.hpp>
#include <villas/node.hpp>
#include <villas/signal.hpp>

namespace villas {
namespace node {
namespace c37_118 {

class C37_118 : public Node {
protected:
	struct Input {
		std::string address;
	} input;

	virtual
	int _read(struct Sample *smps[], unsigned cnt) override;

public:
	C37_118(const std::string &name = "");

	virtual
	~C37_118() override;

	virtual
	int parse(json_t *json, const uuid_t sn_uuid) override;

	virtual
	int start() override;

	virtual
	int stop() override;
};

} /* namespace c37_118 */
} /* namespace node */
} /* namespace villas */
