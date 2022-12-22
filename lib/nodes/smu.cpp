/** Node-type for smu data aquisition.
 *
 * @file
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <list>
#include <cmath>
#include <cstring>

#include <villas/exceptions.hpp>
#include <villas/nodes/smu.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

SMUNode::SMUNode(const std::string &name)
{
    return;
}

int SMUNode::prepare()
{
	assert(state == State::CHECKED);

	return Node::prepare();
}

int SMUNode::start()
{
	assert(state == State::PREPARED ||
	       state == State::PAUSED);

	int ret = Node::start();
	if (!ret)
		state = State::STARTED;

	return ret;
}

int SMUNode::_read(struct Sample *smps[], unsigned cnt)
{
	struct Sample *t = smps[0];

	struct timespec ts;
	ts.tv_sec = time(nullptr);
	ts.tv_nsec = 0;

	assert(cnt == 1);

	t->flags = (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_DATA | (int) SampleFlags::HAS_SEQUENCE;
	t->ts.origin = ts;
	t->sequence = sequence++;
	t->length = 1;
	t->signals = in.signals;
	sleep(1);
	for (unsigned i = 0; i < t->length; i++) {
		t->data[i].f = 1.0;

	}

	return 1;
}

static char n[] = "smu";
static char d[] = "SMU data acquisition";
static NodePlugin<SMUNode, n , d, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_POLL> p;