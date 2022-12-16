/** Drop hook.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <cinttypes>

#include <villas/hook.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class DropHook : public Hook {

protected:
	struct Sample *prev;

public:
	using Hook::Hook;

	virtual void start()
	{
		assert(state == State::PREPARED || state == State::STOPPED);

		prev = nullptr;

		state = State::STARTED;
	}

	virtual void stop()
	{
		assert(state == State::STARTED);

		if (prev)
			sample_decref(prev);

		state = State::STOPPED;
	}

	virtual Hook::Reason process(struct Sample *smp)
	{
		int dist;

		assert(state == State::STARTED);

		if (prev) {
			dist = smp->sequence - (int64_t) prev->sequence;
			if (dist <= 0) {
				logger->debug("Dropping reordered sample: sequence={}, distance={}", smp->sequence, dist);

				return Hook::Reason::SKIP_SAMPLE;
			}
		}

		sample_incref(smp);
		if (prev)
			sample_decref(prev);

		prev = smp;

		return Reason::OK;
	}

	virtual void restart()
	{
		assert(state == State::STARTED);

		if (prev) {
			sample_decref(prev);
			prev = nullptr;
		}
	}
};

/* Register hook */
static char n[] = "drop";
static char d[] = "Drop messages with reordered sequence numbers";
static HookPlugin<DropHook, n, d, (int) Hook::Flags::BUILTIN | (int) Hook::Flags::NODE_READ, 3> p;

} /* namespace node */
} /* namespace villas */
