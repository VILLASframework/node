/** Shift sequence number of samples
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
#include <villas/sample.hpp>

namespace villas {
namespace node {

class ShiftSequenceHook : public Hook {

protected:
	int offset;

public:

	using Hook::Hook;

	virtual void parse(json_t *json)
	{
		json_error_t err;
		int ret;

		assert(state != State::STARTED);

		Hook::parse(json);

		ret = json_unpack_ex(json, &err, 0, "{ s: i }",
			"offset", &offset
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-shift_seq");

		state = State::PARSED;
	}

	virtual Hook::Reason process(struct Sample *smp)
	{
		assert(state == State::STARTED);

		smp->sequence += offset;

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "shift_seq";
static char d[] = "Shift sequence number of samples";
static HookPlugin<ShiftSequenceHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */
