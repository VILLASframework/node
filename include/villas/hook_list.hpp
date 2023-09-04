/* Hook list functions.
 *
 * This file includes some examples.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <jansson.h>

#include <villas/hook.hpp>

namespace villas {
namespace node {

// Forward declarations
class Node;
class Path;
struct Sample;

class HookList : public std::list<Hook::Ptr> {

public:
	HookList()
	{ }

	/* Parses an object of hooks
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

	// Get the maximum number of signals which is used by any of the hooks in the list.
	unsigned getSignalsMaxCount() const;

	json_t * toJson() const;
};

} // namespace node
} // namespace villas
