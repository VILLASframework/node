/** Node type: Universal Data-exchange API
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

#include <vector>
#include <villas/exceptions.hpp>
#include <villas/nodes/api.hpp>

using namespace villas;
using namespace villas::node;

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

static char n[] = "api";
static char d[] = "A node providing a HTTP REST interface";
static NodePlugin<APINode, n , d, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_WRITE, 1> p;
