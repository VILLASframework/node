/** Node direction
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#pragma once

#include <jansson.h>

#include <villas/common.hpp>
#include <villas/list.hpp>
#include <villas/signal_list.hpp>
#include <villas/hook_list.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class Node;
class Path;

class NodeDirection {
	friend Node;

public:
	enum class Direction {
		IN,			/**< VILLASnode is receiving/reading */
		OUT			/**< VILLASnode is sending/writing */
	} direction;

	/** The path which uses this node as a source/destination.
	 *
	 * Usually every node should be used only by a single path as destination.
	 * Otherwise samples from different paths would be interleaved.
	 */
	Path *path;
	Node *node;

	int enabled;
	int builtin;			/**< This node should use built-in hooks by default. */
	unsigned vectorize;		/**< Number of messages to send / recv at once (scatter / gather) */

	HookList hooks;			/**< List of read / write hooks (struct hook). */
	SignalList::Ptr signals;	/**< Signal description. */

	json_t *config;			/**< A JSON object containing the configuration of the node. */

	NodeDirection(enum NodeDirection::Direction dir, Node *n);

	int parse(json_t *json);
	void check();
	int prepare();
	int start();
	int stop();

	SignalList::Ptr getSignals(int after_hooks = true) const;

	unsigned getSignalsMaxCount() const;
};

} /* namespace node */
} /* namespace villas */
