/* Shift sequence number of samples.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/hook.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class ShiftSequenceHook : public Hook {

protected:
	int offset;

public:

	using Hook::Hook;

	virtual
	void parse(json_t *json)
	{
		json_error_t err;
		int ret;

		assert(state != State::STARTED);

		Hook::parse(json);

		ret = json_unpack_ex(json, &err, 0, "{ s: i }",
			"offset", &offset
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-shift_seq");

		state = State::PARSED;
	}

	virtual
	Hook::Reason process(struct Sample *smp)
	{
		assert(state == State::STARTED);

		smp->sequence += offset;

		return Reason::OK;
	}
};

// Register hook
static char n[] = "shift_seq";
static char d[] = "Shift sequence number of samples";
static HookPlugin<ShiftSequenceHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::PATH> p;

} // namespace node
} // namespace villas
