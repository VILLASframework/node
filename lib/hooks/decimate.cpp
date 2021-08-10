/** Decimate hook.
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

#include <villas/hooks/decimate.hpp>

namespace villas {
namespace node {

void DecimateHook::start()
{
	assert(state == State::PREPARED);

	counter = 0;

	state = State::STARTED;
}

void DecimateHook::parse(json_t *json)
{
	int ret;
	json_error_t err;

	assert(state != State::STARTED);

	Hook::parse(json);

	ret = json_unpack_ex(json, &err, 0, "{ s: i }",
		"ratio", &ratio
	);
	if (ret)
		throw ConfigError(json, err, "node-config-hook-decimate");

	state = State::PARSED;
}

Hook::Reason DecimateHook::process(struct Sample *smp)
{
	assert(state == State::STARTED);

	if (ratio && counter++ % ratio != 0)
		return Hook::Reason::SKIP_SAMPLE;

	return Reason::OK;
}

/* Register hook */
static char n[] = "decimate";
static char d[] = "Downsamping by integer factor";
static HookPlugin<DecimateHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */
