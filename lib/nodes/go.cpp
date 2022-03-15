/** Node-type implemeted in Go language
 *
 * @file
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

#include <villas/nodes/go.hpp>
#include <villas/plugin.hpp>
#include <villas/format.hpp>

extern "C" {
	#include <libvillas-go.h>
	#include <villas/nodes/go.h>
}

using namespace villas;
using namespace villas::node;

void _go_register_node_factory(_go_plugin_list pl, char *name, char *desc, int flags)
{
	auto *plugins = (villas::plugin::List<> *) pl;
	plugins->push_back(new villas::node::GoNodeFactory(name, desc, flags));
}

_go_logger * _go_logger_get(char *name)
{
	return (_go_logger *) villas::logging.get(name).get();
}

void _go_logger_log(_go_logger l, int level, char *msg)
{
	auto *log = (spdlog::logger *) l;
	log->log((spdlog::level::level_enum) level, "{}", msg);
}

GoNode::GoNode(uintptr_t n) :
	Node(),
	node(n),
	formatter(nullptr)
{ }

GoNode::~GoNode()
{
	GoNodeClose(node);
}

int GoNode::parse(json_t *json, const uuid_t sn_uuid)
{
	int ret = Node::parse(json, sn_uuid);
	if (ret)
		return ret;

	GoNodeSetLogger(node, _go_logger_log, logger.get());

	json_t *json_format = nullptr;
	json_error_t err;

	ret = json_unpack_ex(json, &err, 0, "{ s?: o }",
		"format", &json_format
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-format", "Failed to parse node configuration");

	formatter = json_format
			? FormatFactory::make(json_format)
			: FormatFactory::make("json");
	if (!formatter)
		throw ConfigError(json_format, "node-config-node-format", "Invalid format configuration");

	auto *cfg = json_dumps(json, JSON_COMPACT);
	ret = GoNodeParse(node, cfg);
	free(cfg);
	return ret;
}

int GoNode::check()
{
	int ret = Node::check();
	if (ret)
		return ret;

	return GoNodeCheck(node);
}

int GoNode::prepare()
{
	int ret = GoNodePrepare(node);
	if (ret)
		return ret;

	return Node::prepare();
}

int GoNode::start()
{
	int ret;

	assert(state == State::PREPARED ||
	       state == State::PAUSED);

	/* Initialize IO */
	formatter->start(getInputSignals(false));

	ret = GoNodeStart(node);
	if (ret)
		return ret;

	ret = Node::start();
	if (!ret)
		state = State::STARTED;

	return ret;
}

int GoNode::stop() {
	int ret;

	assert(state == State::STARTED ||
	       state == State::PAUSED ||
	       state == State::STOPPING);

	ret = Node::stop();
	if (ret)
		return ret;

	ret = GoNodeStop(node);
	if (ret)
		return ret;

	return 0;
}

int GoNode::pause()
{
	int ret;

	ret = Node::pause();
	if (ret)
		return ret;

	ret = GoNodePause(node);
	if (!ret)
		state = State::PAUSED;

	return ret;
}

int GoNode::resume()
{
	int ret;

	ret = Node::resume();
	if (ret)
		return ret;

	ret = GoNodeResume(node);
	if (!ret)
		state = State::STARTED;

	return ret;
}

const std::string & GoNode::getDetails()
{
	if (_details.empty()) {
		auto *d = GoNodeDetails(node);
		_details = std::string(d);
		free(d);
	}

	return _details;
}

std::vector<int> GoNode::getPollFDs()
{
	auto ret = GoNodeGetPollFDs(node);
	if (ret.r1)
		return {};

	auto begin = (int *) ret.r0.data;
	auto end = begin + ret.r0.len;

	return std::vector<int>(begin, end);
}

std::vector<int> GoNode::getNetemFDs()
{
	auto ret = GoNodeGetNetemFDs(node);
	if (ret.r1)
		return {};

	auto begin = (int *) ret.r0.data;
	auto end = begin + ret.r0.len;

	return std::vector<int>(begin, end);
}

int GoNode::_read(struct Sample * smps[], unsigned cnt)
{
	int ret;
	char buf[4096];
	size_t rbytes;

	auto d = GoNodeRead(node, buf, 4096);
	if (d.r1)
		return d.r1;

	ret = formatter->sscan(buf, d.r0, &rbytes, smps, cnt);
	if (ret < 0 || (size_t) d.r0 != rbytes) {
		logger->warn("Received invalid packet: ret={}, bytes={}, rbytes={}", ret, d.r0, rbytes);
		return ret;
	}

	return ret;
}

int GoNode::_write(struct Sample * smps[], unsigned cnt)
{
	int ret;
	char buf[4096];
	size_t wbytes;

	ret = formatter->sprint(buf, 4096, &wbytes, smps, cnt);
	if (ret < 0)
		return ret;

	GoSlice slice = {
		data: buf,
		len: GoInt(wbytes),
		cap: 4096
	};

	ret = GoNodeWrite(node, slice);
	if (ret)
		return ret;

	return cnt;
}

int GoNode::restart()
{
	assert(state == State::STARTED);

	logger->info("Restarting node");

	return GoNodeRestart(node);
}

int GoNode::reverse()
{
	return GoNodeReverse(node);
}


Node * GoNodeFactory::make()
{
	auto nt = NewGoNode((char *) name.c_str());
	if (!nt)
		return nullptr;

	auto *n = new GoNode(nt);

	init(n);

	GoNodeSetLogger(n->node, _go_logger_log, n->logger.get());

	return n;
}

GoPluginRegistry::GoPluginRegistry() {
	if (plugin::registry == nullptr)
		plugin::registry = new plugin::Registry();

	plugin::registry->addSubRegistry(this);
}

villas::plugin::List<> GoPluginRegistry::lookup()
{
	plugin::List<> plugins;

	RegisterGoNodeTypes(_go_register_node_factory, &plugins);

	return plugins;
}

static GoPluginRegistry pr;
