/** An example get started with new implementations of new node-types
 *
 * This example does not do any particulary useful.
 * It is just a skeleton to get you started with new node-types.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/node_compat.hpp>
#include <villas/nodes/example.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

ExampleNode::ExampleNode(const std::string &name) :
	Node(name),
	setting1(72),
	setting2("something"),
	state1(0)
{ }

ExampleNode::~ExampleNode()
{ }

int ExampleNode::prepare()
{
	state1 = setting1;

	if (setting2 == "double")
		state1 *= 2;

	return 0;
}

int ExampleNode::parse(json_t *json, const uuid_t sn_uuid)
{
	/* TODO: Add implementation here. The following is just an example */

	const char *setting2_str = nullptr;

	json_error_t err;
	int ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: s }",
		"setting1", &setting1,
		"setting2", &setting2_str
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-example");

	if (setting2_str)
		setting2 = setting2_str;

	return 0;
}

int ExampleNode::check()
{
	if (setting1 > 100 || setting1 < 0)
		return -1;

	if (setting2.empty() || setting2.size() > 10)
		return -1;

	return 0;
}

int ExampleNode::start()
{
	// TODO add implementation here

	start_time = time_now();

	return 0;
}

// int ExampleNode::stop()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int ExampleNode::pause()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int ExampleNode::resume()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int ExampleNode::restart()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int ExampleNode::reverse()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// std::vector<int> ExampleNode::getPollFDs()
// {
// 	// TODO add implementation here
// 	return {};
// }

// std::vector<int> ExampleNode::getNetemFDs()
// {
// 	// TODO add implementation here
// 	return {};
// }

// struct villas::node::memory::Type * ExampleNode::getMemoryType()
// {
//	// TODO add implementation here
// }

const std::string & ExampleNode::getDetails()
{
	details = fmt::format("setting1={}, setting2={}", setting1, setting2);
	return details;
}

int ExampleNode::_read(struct Sample *smps[], unsigned cnt)
{
	int read;
	struct timespec now;

	/* TODO: Add implementation here. The following is just an example */

	assert(cnt >= 1 && smps[0]->capacity >= 1);

	now = time_now();

	smps[0]->data[0].f = time_delta(&now, &start_time);

	/* Dont forget to set other flags in struct Sample::flags
	 * E.g. for sequence no, timestamps... */
	smps[0]->flags = (int) SampleFlags::HAS_DATA;
	smps[0]->signals = getInputSignals(false);

	read = 1; /* The number of samples read */

	return read;
}

int ExampleNode::_write(struct Sample *smps[], unsigned cnt)
{
	int written;

	/* TODO: Add implementation here. */

	written = 0; /* The number of samples written */

	return written;
}


static char n[] = "example";
static char d[] = "An example for staring new node-type implementations";
static NodePlugin<ExampleNode, n , d, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_WRITE | (int) NodeFactory::Flags::SUPPORTS_POLL | (int) NodeFactory::Flags::HIDDEN> p;
