/** Rate-limiting hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

/** @addtogroup hooks Hook functions
 * @{
 */

#include <string.h>

#include <villas/timing.h>
#include <villas/sample.h>
#include <villas/hooks/limit_rate.hpp>

namespace villas {
namespace node {

void LimitRateHook::parse(json_t *cfg)
{
	int ret;
	json_error_t err;

	assert(state != STATE_STARTED);

	double rate;
	const char *m = nullptr;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: F, s?: s }",
		"rate", &rate,
		"mode", &m
	);
	if (ret)
		throw ConfigError(cfg, err, "node-config-hook-limit_rate");

	if (m) {
		if (!strcmp(m, "origin"))
			mode = LIMIT_RATE_ORIGIN;
		else if (!strcmp(m, "received"))
			mode = LIMIT_RATE_RECEIVED;
		else if (!strcmp(m, "local"))
			mode = LIMIT_RATE_LOCAL;
		else
			throw ConfigError(cfg, "node-config-hook-limit_rate-mode", "Invalid value '{}' for setting 'mode'", mode);
	}

	deadtime = 1.0 / rate;

	state = STATE_PARSED;
}

int LimitRateHook::process(sample *smp)
{
	assert(state == STATE_STARTED);

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
		return HOOK_SKIP_SAMPLE;

	last = next;

	return HOOK_OK;
}

/* Register hook */
static HookPlugin<LimitRateHook> p(
	"limit_rate",
	"Limit sending rate",
	HOOK_NODE_READ | HOOK_NODE_WRITE | HOOK_PATH,
	99
);

} /* namespace node */
} /* namespace villas */

/** @} */
