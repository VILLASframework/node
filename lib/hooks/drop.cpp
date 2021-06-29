/** Drop hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <cinttypes>

#include <villas/hook.hpp>
#include <villas/node.h>
#include <villas/sample.h>

namespace villas {
namespace node {

class DropHook : public Hook {

protected:
	sample *prev;

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

	virtual Hook::Reason process(struct sample *smp)
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
static HookPlugin<DropHook, n, d, (int) Hook::Flags::READ_ONLY | (int) Hook::Flags::BUILTIN | (int) Hook::Flags::NODE_READ, 3> p;

} /* namespace node */
} /* namespace villas */

/** @} */
