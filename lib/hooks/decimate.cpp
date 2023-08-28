/** Decimate hook.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/hooks/decimate.hpp>
#include <villas/sample.hpp>

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

	ret = json_unpack_ex(json, &err, 0, "{ s: i, s?: b }",
		"ratio", &ratio,
		"renumber", &renumber
	);
	if (ret)
		throw ConfigError(json, err, "node-config-hook-decimate");

	state = State::PARSED;
}

Hook::Reason DecimateHook::process(struct Sample *smp)
{
	assert(state == State::STARTED);

	if (renumber)
		smp->sequence /= ratio;

	if (ratio && counter++ % ratio != 0)
		return Hook::Reason::SKIP_SAMPLE;

	return Reason::OK;
}

/* Register hook */
static char n[] = "decimate";
static char d[] = "Downsamping by integer factor";
static HookPlugin<DecimateHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} // namespace node
} // namespace villas
