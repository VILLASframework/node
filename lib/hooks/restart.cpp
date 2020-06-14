/** Path restart hook.
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

class RestartHook : public Hook {

protected:
	sample *prev;

public:
	using Hook::Hook;

	virtual void start()
	{
		assert(state == State::PREPARED);

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

	virtual Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);

		if (prev) {
			/* A wrap around of the sequence no should not be treated as a simulation restart */
			if (smp->sequence == 0 && prev->sequence != 0 && prev->sequence < UINT64_MAX - 16) {
				logger->warn("Simulation from node {} restarted (previous->sequence={}, current->sequence={})",
					node_name(node), prev->sequence, smp->sequence);

				smp->flags |= (int) SampleFlags::IS_FIRST;

				/* Restart hooks */
				for (size_t i = 0; i < vlist_length(&node->in.hooks); i++) {
					Hook *k = (Hook *) vlist_at(&node->in.hooks, i);

					k->restart();
				}

				for (size_t i = 0; i < vlist_length(&node->out.hooks); i++) {
					Hook *k = (Hook *) vlist_at(&node->out.hooks, i);

					k->restart();
				}
			}
		}

		sample_incref(smp);
		if (prev)
			sample_decref(prev);

		prev = smp;

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "restart";
static char d[] = "Call restart hooks for current node";
static HookPlugin<RestartHook, n, d, (int) Hook::Flags::BUILTIN | (int) Hook::Flags::NODE_READ, 1> p;

} /* namespace node */
} /* namespace villas */

/** @} */
