
/** Dump hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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
