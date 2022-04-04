/** Node type: IEC 61850-8-1 (GOOSE)
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2017-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/nodes/iec61850_goose.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::iec61850;

GooseNode::GooseNode(const std::string &name) :
	Node(name)
{
	// TODO
}

GooseNode::~GooseNode()
{
	// TODO
}

void GooseNode::listenerStatic(SVSubscriber subscriber, void *ctx, SVSubscriber_ASDU asdu)
{
	auto *n = static_cast<GooseNode *>(ctx);
	n->listener(subscriber, asdu);
}

void GooseNode::listener(SVSubscriber subscriber, SVSubscriber_ASDU asdu)
{
	// TODO
}

int GooseNode::parse(json_t *json, const uuid_t sn_uuid)
{
	return -1; // TODO
}

std::string GooseNode::getDetails()
{
	auto details = ""; // TODO

	return details;
}

int GooseNode::start()
{
	/* Initialize publisher */
	if (out.enabled) {
		 // TODO
	}

	/* Start subscriber */
	if (in.enabled) {
		 // TODO
	}

	return 0;
}

int GooseNode::stop()
{
	return -1; // TODO
}

int GooseNode::_read(struct Sample *smps[], unsigned cnt)
{
	if (!in.enabled)
		return -1;

	return -1; // TODO
}

int GooseNode::_write(struct Sample *smps[], unsigned cnt)
{
	if (!out.enabled)
		return -1;

	return -1; // TODO
}

std::vector<int> GooseNode::getPollFDs()
{
	return { }; // TODO
}

static char n[] = "iec61850-8-1";
static char d[] = "IEC 61850-8-1 (GOOSE)";
static NodePlugin<GooseNode, n, d, (int) NodeFactory::Flags::SUPPORTS_POLL |
                                (int) NodeFactory::Flags::SUPPORTS_READ |
                                (int) NodeFactory::Flags::SUPPORTS_WRITE> nf;
