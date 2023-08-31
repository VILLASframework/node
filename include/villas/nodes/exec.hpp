/* Node-type for exec node-types.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/node.hpp>
#include <villas/popen.hpp>
#include <villas/format.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class ExecNode : public Node {

protected:
	std::unique_ptr<villas::utils::Popen> proc;
	std::unique_ptr<Format> formatter;

	FILE *stream_in, *stream_out;

	bool flush;
	bool shell;

	std::string working_dir;
	std::string command;

	villas::utils::Popen::arg_list arguments;
	villas::utils::Popen::env_map environment;

	virtual
	int _read(struct Sample * smps[], unsigned cnt);

	virtual
	int _write(struct Sample * smps[], unsigned cnt);

public:
	ExecNode(const uuid_t &id = {}, const std::string &name = "") :
		Node(id, name),
		stream_in(nullptr),
		stream_out(nullptr),
		flush(true),
		shell(false)
	{ }

	virtual
	~ExecNode();

	virtual
	const std::string & getDetails();

	virtual
	int start();

	virtual
	int stop();

	virtual
	int prepare();

	virtual
	int parse(json_t *json);

	virtual
	std::vector<int> getPollFDs();
};

} // namespace node
} // namespace villas
