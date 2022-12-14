/** Node type: Universal Data-exchange API (v2)
 *
 * @see https://github.com/ERIGrid2/JRA-3.1-api
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <vector>

#include <villas/exceptions.hpp>
#include <villas/api/universal.hpp>
#include <villas/nodes/api.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::api::universal;

APINode::APINode(const std::string &name) :
	Node(name),
	read(),
	write()
{
	int ret;
	auto dirs = std::vector{&read, &write};

	for (auto dir : dirs) {
		ret = pthread_mutex_init(&dir->mutex, nullptr);
		if (ret)
			throw RuntimeError("failed to initialize mutex");

		ret = pthread_cond_init(&dir->cv, nullptr);
		if (ret)
			throw RuntimeError("failed to initialize mutex");
	}
}

int APINode::prepare()
{
	auto signals_in = getInputSignals(false);

	read.sample = sample_alloc_mem(signals_in->size());
	if (!read.sample)
		throw MemoryAllocationError();

	write.sample = sample_alloc_mem(64);
	if (!write.sample)
		throw MemoryAllocationError();

	unsigned j = 0;
	for (auto sig : *signals_in)
		read.sample->data[j++] = sig->init;

	read.sample->length = j;
	read.sample->signals = signals_in;

	return Node::prepare();
}

int APINode::check()
{
	for (auto &ch : read.channels) {
		if (ch->payload != PayloadType::SAMPLES)
			return -1;
	}

	for (auto &ch : write.channels) {
		if (ch->payload != PayloadType::SAMPLES)
			return -1;
	}

	return 0;
}

int APINode::_read(struct Sample *smps[], unsigned cnt)
{
	assert(cnt == 1);

	pthread_cond_wait(&read.cv, &read.mutex);

	sample_copy(smps[0], read.sample);

	return 1;
}

int APINode::_write(struct Sample *smps[], unsigned cnt)
{
	assert(cnt == 1);

	sample_copy(write.sample, smps[0]);

	pthread_cond_signal(&write.cv);

	return 1;
}

int APINode::parse(json_t *json, const uuid_t sn_uuid)
{
	int ret = Node::parse(json, sn_uuid);
	if (ret)
		return ret;

	json_t *json_signals_in = nullptr;
	json_t *json_signals_out = nullptr;

	json_error_t err;
	ret = json_unpack_ex(json, &err, 0, "{ s?: { s?: o }, s?: { s?: o } }",
		"in",
			"signals", &json_signals_in,
		"out",
			"signals", &json_signals_out
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-api");

	if (json_signals_in)
		read.channels.parse(json_signals_in, false, true);

	if (json_signals_out)
		write.channels.parse(json_signals_out, true, false);

	return 0;
}

static char n[] = "api";
static char d[] = "A node providing a HTTP REST interface";
static NodePlugin<APINode, n , d, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_WRITE, 1> p;
