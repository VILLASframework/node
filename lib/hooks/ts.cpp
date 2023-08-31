/* Timestamp hook.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/hook.hpp>
#include <villas/timing.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class TsHook : public Hook {

public:
	using Hook::Hook;

	virtual
	Hook::Reason process(struct Sample *smp)
	{
		assert(state == State::STARTED);

		smp->ts.origin = smp->ts.received;

		return Reason::OK;
	}
};

// Register hook
static char n[] = "ts";
static char d[] = "Overwrite origin timestamp of samples with receive timestamp";
static HookPlugin<TsHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} // namespace node
} // namespace villas
