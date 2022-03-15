/** Timestamp hook.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/hook.hpp>
#include <villas/timing.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class TsHook : public Hook {

public:
	using Hook::Hook;

	virtual Hook::Reason process(struct Sample *smp)
	{
		assert(state == State::STARTED);

		smp->ts.origin = smp->ts.received;

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "ts";
static char d[] = "Overwrite origin timestamp of samples with receive timestamp";
static HookPlugin<TsHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */
