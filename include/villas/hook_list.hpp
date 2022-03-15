/** Hook list functions
 *
 * This file includes some examples.
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
 */

#pragma once

#include <jansson.h>

#include <villas/hook.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class Node;
class Path;
struct Sample;

class HookList : public std::list<Hook::Ptr> {

public:
	HookList()
	{ }

	/** Parses an object of hooks
	 *
	 * Example:
	 *
	 * {
	 *    stats = {
	 *       output = "stdout"
	 *    },
	 *    skip_first = {
	 *       seconds = 10
	 *    },
	 *    hooks = [ "print" ]
	 * }
	 */
	void parse(json_t *json, int mask, Path *p, Node *n);

	void check();

	void prepare(SignalList::Ptr sigs, int mask, Path *p, Node *n);

	int process(struct Sample *smps[], unsigned cnt);
	void periodic();
	void start();
	void stop();

	void dump(villas::Logger logger, std::string subject) const;

	SignalList::Ptr getSignals() const;

	/** Get the maximum number of signals which is used by any of the hooks in the list. */
	unsigned getSignalsMaxCount() const;

	json_t * toJson() const;
};

} /* namespace node */
} /* namespace villas */
