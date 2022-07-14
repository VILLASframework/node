/** The WebRTC node-type.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/node_compat.hpp>
#include <villas/nodes/webrtc.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

WebRTCNode::WebRTCNode(const std::string &name) :
	Node(name)
{ }

WebRTCNode::~WebRTCNode()
{ }

int WebRTCNode::prepare()
{
	// state1 = setting1;

	// if (setting2 == "double")
	// 	state1 *= 2;

	return 0;
}

int WebRTCNode::parse(json_t *json, const uuid_t sn_uuid)
{
	/* TODO: Add implementation here. The following is just an example */

	// const char *setting2_str = nullptr;

	// json_error_t err;
	// int ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: s }",
	// 	"setting1", &setting1,
	// 	"setting2", &setting2_str
	// );
	// if (ret)
	// 	throw ConfigError(json, err, "node-config-node-example");

	// if (setting2_str)
	// 	setting2 = setting2_str;

	return 0;
}

int WebRTCNode::check()
{
	return 0;
}

int WebRTCNode::start()
{
	// TODO add implementation here

	// start_time = time_now();

	return 0;
}

// int WebRTCNode::stop()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int WebRTCNode::pause()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int WebRTCNode::resume()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int WebRTCNode::restart()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int WebRTCNode::reverse()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// std::vector<int> WebRTCNode::getPollFDs()
// {
// 	// TODO add implementation here
// 	return {};
// }

// std::vector<int> WebRTCNode::getNetemFDs()
// {
// 	// TODO add implementation here
// 	return {};
// }

// struct villas::node::memory::Type * WebRTCNode::getMemoryType()
// {
//	// TODO add implementation here
// }

const std::string & WebRTCNode::getDetails()
{
	details = fmt::format("");
	return details;
}

int WebRTCNode::_read(struct Sample *smps[], unsigned cnt)
{
	int read;
	struct timespec now;

	/* TODO: Add implementation here. The following is just an example */

	assert(cnt >= 1 && smps[0]->capacity >= 1);

	now = time_now();

	// smps[0]->data[0].f = time_delta(&now, &start_time);

	/* Dont forget to set other flags in struct Sample::flags
	 * E.g. for sequence no, timestamps... */
	smps[0]->flags = (int) SampleFlags::HAS_DATA;
	smps[0]->signals = getInputSignals(false);

	read = 1; /* The number of samples read */

	return read;
}

int WebRTCNode::_write(struct Sample *smps[], unsigned cnt)
{
	int written;

	/* TODO: Add implementation here. */

	written = 0; /* The number of samples written */

	return written;
}


static char n[] = "webrtc";
static char d[] = "Web Real-time Communication";
static NodePlugin<WebRTCNode, n , d, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_WRITE | (int) NodeFactory::Flags::SUPPORTS_POLL | (int) NodeFactory::Flags::HIDDEN> p;
