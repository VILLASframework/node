/* Node-type for loopback connections.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <jansson.h>

#include <villas/queue_signalled.h>

namespace villas {
namespace node {

class LoopbackNode : public Node {

protected:
	int queuelen;
	struct CQueueSignalled queue;
	enum QueueSignalledMode mode;

	virtual
	int _write(struct Sample * smps[], unsigned cnt);

	virtual
	int _read(struct Sample * smps[], unsigned cnt);

public:
	LoopbackNode(const uuid_t &id = {}, const std::string &name = "");

	virtual
	~LoopbackNode();

	virtual
	int prepare();

	virtual
	int stop();

	virtual
	std::vector<int> getPollFDs();

	virtual
	const std::string & getDetails();

	virtual
	int parse(json_t *json);
};

} // namespace node
} // namespace villas
