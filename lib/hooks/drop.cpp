/** Drop hook.
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

#include <inttypes.h>

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
		assert(state == STATE_PREPARED || state == STATE_STOPPED);

		prev = NULL;

		state = STATE_STARTED;
	}

	virtual void stop()
	{
		assert(state == STATE_STARTED);

		if (prev)
			sample_decref(prev);

		state = STATE_STOPPED;
	}

	virtual int process(sample *smp)
	{
		int dist;

		assert(state == STATE_STARTED);

		if (prev) {
			dist = smp->sequence - (int64_t) prev->sequence;
			if (dist <= 0) {
				logger->debug("Dropping reordered sample: sequence={}, distance={}", smp->sequence, dist);

				return HOOK_SKIP_SAMPLE;
			}
		}

		sample_incref(smp);
		if (prev)
			sample_decref(prev);

		prev = smp;

		return HOOK_OK;
	}

	virtual void restart()
	{
		assert(state == STATE_STARTED);

		if (prev) {
			sample_decref(prev);
			prev = NULL;
		}
	}
};

/* Register hook */
static HookPlugin<DropHook> p(
	"drop",
	"Drop messages with reordered sequence numbers",
	HOOK_BUILTIN | HOOK_NODE_READ,
	3
);

} /* namespace node */
} /* namespace villas */

/** @} */
