
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
#include <villas/timing.h>

namespace villas {
namespace node {

class FixHook : public Hook {

public:
	using Hook::Hook;

	virtual int process(sample *smp)
	{
		assert(state == STATE_STARTED);

		timespec now = time_now();

		if (!(smp->flags & SAMPLE_HAS_SEQUENCE) && node) {
			smp->sequence = node->sequence++;
			smp->flags |= SAMPLE_HAS_SEQUENCE;
		}

		if (!(smp->flags & SAMPLE_HAS_TS_RECEIVED)) {
			smp->ts.received = now;
			smp->flags |= SAMPLE_HAS_TS_RECEIVED;
		}

		if (!(smp->flags & SAMPLE_HAS_TS_ORIGIN)) {
			smp->ts.origin = smp->ts.received;
			smp->flags |= SAMPLE_HAS_TS_ORIGIN;
		}

		return HOOK_OK;
	}
};

/* Register hook */
static HookPlugin<FixHook> p(
	"fix",
	"Fix received data by adding missing fields",
	HOOK_BUILTIN | HOOK_NODE_READ,
	1
);

} /* namespace node */
} /* namespace villas */

/** @} */

