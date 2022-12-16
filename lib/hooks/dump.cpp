/** Dump hook.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/hook.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class DumpHook : public Hook {

public:
	using Hook::Hook;

	virtual Hook::Reason process(struct Sample *smp)
	{
		assert(state == State::STARTED);

		sample_dump(logger, smp);

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "dump";
static char d[] = "Dump data to stdout";
static HookPlugin<DumpHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH, 1> p;

} /* namespace node */
} /* namespace villas */
