/** Node-type for internal loopback_internal connections.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstring>

#include <villas/node/config.hpp>
#include <villas/nodes/loopback_internal.hpp>
#include <villas/exceptions.hpp>
#include <villas/node/memory.hpp>
#include <villas/uuid.hpp>
#include <villas/colors.hpp>

using namespace villas;
using namespace villas::node;

static InternalLoopbackNodeFactory nf;

InternalLoopbackNode::InternalLoopbackNode(Node *src, unsigned id, unsigned ql) :
	Node(fmt::format("{}.lo{}", src->getNameShort(), id)),
	queuelen(ql),
	source(src)
{
	int ret = uuid::generateFromString(uuid, fmt::format("lo{}", id), src->getUuid());
	if (ret)
		throw RuntimeError("Failed to initialize UUID");

	factory = &nf;
	name_long = fmt::format(CLR_RED("{}") "(" CLR_YEL("{}") ")", name_short, nf.getName());

	auto logger_name = fmt::format("node:{}", getNameShort());

	logger = logging.get(logger_name);

	in.signals = source->getInputSignals(false);

	ret = queue_signalled_init(&queue, queuelen);
	if (ret)
		throw RuntimeError("Failed to initialize queue");

	state = State::PREPARED;
}

InternalLoopbackNode::~InternalLoopbackNode()
{
	int ret __attribute__((unused));

	ret = queue_signalled_destroy(&queue);
}

int InternalLoopbackNode::stop()
{
	int ret;

	ret = queue_signalled_close(&queue);
	if (ret)
		return ret;

	return 0;
}

int InternalLoopbackNode::_read(struct Sample * smps[], unsigned cnt)
{
	int avail;

	struct Sample *cpys[cnt];

	avail = queue_signalled_pull_many(&queue, (void **) cpys, cnt);

	sample_copy_many(smps, cpys, avail);
	sample_decref_many(cpys, avail);

	return avail;
}

int InternalLoopbackNode::_write(struct Sample * smps[], unsigned cnt)
{
	sample_incref_many(smps, cnt);

	int pushed = queue_signalled_push_many(&queue, (void **) smps, cnt);
	if (pushed < 0) {
		sample_decref_many(smps, cnt);
		return pushed;
	}

	/* Released unpushed samples */
	if ((unsigned) pushed < cnt) {
		sample_decref_many(smps + pushed, cnt - pushed);
		logger->warn("Queue overrun");
	}

	return pushed;
}

std::vector<int> InternalLoopbackNode::getPollFDs()
{
	return { queue_signalled_fd(&queue) };
}
