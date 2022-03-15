/** Node-type for exec node-types.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/node.hpp>
#include <villas/popen.hpp>
#include <villas/format.hpp>

namespace villas {
namespace node {

/* Forward declarations */
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
	ExecNode(const std::string &name = "") :
		Node(name),
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
	int parse(json_t *json, const uuid_t sn_uuid);

	virtual
	std::vector<int> getPollFDs();
};

} /* namespace node */
} /* namespace villas */
