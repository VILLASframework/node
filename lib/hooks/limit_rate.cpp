/* Rate-limiting hook.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>

#include <villas/timing.hpp>
#include <villas/sample.hpp>
#include <villas/hooks/limit_rate.hpp>

namespace villas {
namespace node {

void LimitRateHook::parse(json_t *json)
{
	int ret;
	json_error_t err;

	assert(state != State::STARTED);

	double rate;
	const char *m = nullptr;

	Hook::parse(json);

	ret = json_unpack_ex(json, &err, 0, "{ s: F, s?: s }",
		"rate", &rate,
		"mode", &m
	);
	if (ret)
		throw ConfigError(json, err, "node-config-hook-limit_rate");

	if (m) {
		if (!strcmp(m, "origin"))
			mode = LIMIT_RATE_ORIGIN;
		else if (!strcmp(m, "received"))
			mode = LIMIT_RATE_RECEIVED;
		else if (!strcmp(m, "local"))
			mode = LIMIT_RATE_LOCAL;
		else
			throw ConfigError(json, "node-config-hook-limit_rate-mode", "Invalid value '{}' for setting 'mode'", mode);
	}

	deadtime = 1.0 / rate;

	state = State::PARSED;
}

Hook::Reason LimitRateHook::process(struct Sample *smp)
{
	assert(state == State::STARTED);

	timespec next;
	switch (mode) {
		case LIMIT_RATE_LOCAL:
			next = time_now();
			break;

		case LIMIT_RATE_ORIGIN:
			next = smp->ts.origin;
			break;

		case LIMIT_RATE_RECEIVED:
			next = smp->ts.received;
			break;
	}

	if (time_delta(&last, &next) < deadtime)
		return Hook::Reason::SKIP_SAMPLE;

	last = next;

	return Reason::OK;
}

// Register hook
static char n[] = "limit_rate";
static char d[] = "Limit sending rate";
static HookPlugin<LimitRateHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} // namespace node
} // namespace villas
