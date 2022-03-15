/** Node-type for subprocess node-types.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
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

#include <string>
#include <unistd.h>

#include <villas/node/config.hpp>
#include <villas/nodes/exec.hpp>
#include <villas/utils.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/format.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

ExecNode::~ExecNode()
{
	if (stream_in)
		fclose(stream_in);

	if (stream_out)
		fclose(stream_out);
}

int ExecNode::parse(json_t *json, const uuid_t sn_uuid)
{
	int ret = Node::parse(json, sn_uuid);
	if (ret)
		return ret;

	json_error_t err;
	int f = 1, s = -1;

	json_t *json_exec;
	json_t *json_env = nullptr;
	json_t *json_format = nullptr;

	const char *wd = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s: o, s?: o, s?: b, s?: o, s?: b, s?: s }",
		"exec", &json_exec,
		"format", &json_format,
		"flush", &f,
		"environment", &json_env,
		"shell", &s,
		"working_directory", &wd
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-exec");

	flush = f != 0;
	shell = s < 0 ? json_is_string(json_exec) : s != 0;

	arguments.clear();
	environment.clear();

	if (json_is_string(json_exec)) {
		if (!shell)
			throw ConfigError(json_exec, "node-config-node-exec-shell", "The exec setting must be an array if shell mode is disabled.");

		command = json_string_value(json_exec);
	}
	else if (json_is_array(json_exec)) {
		if (shell)
			throw ConfigError(json_exec, "node-config-node-exec-shell", "The exec setting must be a string if shell mode is enabled.");

		if (json_array_size(json_exec) < 1)
			throw ConfigError(json_exec, "node-config-node-exec-exec", "At least one argument must be given");

		size_t i;
		json_t *json_arg;
		json_array_foreach(json_exec, i, json_arg) {
			if (!json_is_string(json_arg))
				throw ConfigError(json_arg, "node-config-node-exec-exec", "All arguments must be of string type");

			if (i == 0)
				command = json_string_value(json_arg);

			arguments.push_back(json_string_value(json_arg));
		}
	}

	if (json_env) {
		/* obj is a JSON object */
		const char *key;
		json_t *json_value;

		json_object_foreach(json_env, key, json_value) {
			if (!json_is_string(json_value))
				throw ConfigError(json_value, "node-config-node-exec-environment", "Environment variables must be of string type");

			environment[key] = json_string_value(json_value);
		}
	}

	/* Format */
	auto *fmt = json_format
			? FormatFactory::make(json_format)
			: FormatFactory::make("villas.human");

	formatter = Format::Ptr(fmt);
	if (!formatter)
		throw ConfigError(json_format, "node-config-node-exec-format", "Invalid format configuration");

	state = State::PARSED;

	return 0;
}

int ExecNode::prepare()
{
	assert(state == State::CHECKED);

	/* Initialize IO */
	formatter->start(getInputSignals(false));

	return Node::prepare();
}

int ExecNode::start()
{
	/* Start subprocess */
	proc = std::make_unique<Popen>(command, arguments, environment, working_dir, shell);
	logger->debug("Started sub-process with pid={}", proc->getPid());

	stream_in = fdopen(proc->getFdIn(), "r");
	if (!stream_in)
		return -1;

	stream_out = fdopen(proc->getFdOut(), "w");
	if (!stream_out)
		return -1;

	int ret = Node::start();
	if (!ret)
		state = State::STARTED;

	return 0;
}

int ExecNode::stop()
{
	int ret = Node::stop();
	if (ret)
		return ret;

	/* Stop subprocess */
	logger->debug("Killing sub-process with pid={}", proc->getPid());
	proc->kill(SIGINT);

	logger->debug("Waiting for sub-process with pid={} to terminate", proc->getPid());
	proc->close();

	/** @todo Check exit code of subprocess? */
	return 0;
}

int ExecNode::_read(struct Sample * smps[], unsigned cnt)
{
	return formatter->scan(stream_in, smps, cnt);
}

int ExecNode::_write(struct Sample * smps[], unsigned cnt)
{
	int ret;

	ret = formatter->print(stream_out, smps, cnt);
	if (ret < 0)
		return ret;

	if (flush)
		fflush(stream_out);

	return cnt;
}

const std::string & ExecNode::getDetails()
{
	std::string wd = working_dir;
	if (wd.empty()) {
		char buf[128];
		wd = getcwd(buf, sizeof(buf));
	}

	if (details.empty()) {
		details = fmt::format("exec={}, shell={}, flush={}, #environment={}, #arguments={}, working_dir={}",
			command,
			shell ? "yes" : "no",
			flush ? "yes" : "no",
			environment.size(),
			arguments.size(),
			wd
		);
	}

	return details;
}

std::vector<int> ExecNode::getPollFDs()
{
	return { proc->getFdIn() };
}

static char n[] = "exec";
static char d[] = "run subprocesses with stdin/stdout communication";
static NodePlugin<ExecNode, n , d, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_WRITE | (int) NodeFactory::Flags::SUPPORTS_POLL> p;
