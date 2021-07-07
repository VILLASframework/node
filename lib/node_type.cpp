/** Nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/sample.h>
#include <villas/node.h>
#include <villas/utils.hpp>
#include <villas/colors.hpp>
#include <villas/node/config.h>

using namespace villas;

/** Global list of all known plugins */
std::list<struct vnode_type *> *node_types = nullptr;

int node_type_start(struct vnode_type *vt, villas::node::SuperNode *sn)
{
	int ret;

	if (vt->state == State::STARTED)
		return 0;

	auto logger = logging.get(fmt::format("node:{}", *vt));
	logger->info("Initializing node type which is used by {} nodes", vt->instances.size());

	ret = vt->type.start ? vt->type.start(sn) : 0;
	if (ret == 0)
		vt->state = State::STARTED;

	return ret;
}

int node_type_stop(struct vnode_type *vt)
{
	int ret;

	if (vt->state != State::STARTED)
		return 0;

	auto logger = logging.get(fmt::format("node:{}", *vt));
	logger->info("De-initializing node type");

	ret = vt->type.stop ? vt->type.stop() : 0;
	if (ret == 0)
		vt->state = State::STOPPED;

	return ret;
}

const char * node_type_name(struct vnode_type *vt)
{
	return vt->name;
}

struct vnode_type * node_type_lookup(const std::string &name)
{
	for (auto *vt : *node_types) {
		if (name == vt->name)
			return vt;
	}

	return nullptr;
}
