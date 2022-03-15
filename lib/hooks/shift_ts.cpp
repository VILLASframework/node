/** Shift timestamps of samples.
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

#include <cstring>

#include <villas/hook.hpp>
#include <villas/timing.hpp>
#include <villas/sample.hpp>

namespace villas {
namespace node {

class ShiftTimestampHook : public Hook {

protected:
	timespec offset;
	enum {
		SHIFT_ORIGIN,
		SHIFT_RECEIVED
	} mode;

public:
	ShiftTimestampHook(Path *p, Node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		mode(SHIFT_ORIGIN)
	{ }

	virtual void parse(json_t *json)
	{
		double o;
		const char *m = nullptr;
		int ret;
		json_error_t err;

		assert(state != State::STARTED);

		Hook::parse(json);

		ret = json_unpack_ex(json, &err, 0, "{ s?: s, s: F }",
			"mode", &m,
			"offset", &o
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-shift_ts");

		if (m) {
			if      (!strcmp(m, "origin"))
				mode = SHIFT_ORIGIN;
			else if (!strcmp(m, "received"))
				mode = SHIFT_RECEIVED;
			else
				throw ConfigError(json, "node-config-hook-shift_ts-mode", "Invalid mode parameter '{}'", m);
		}

		offset = time_from_double(o);

		state = State::PARSED;
	}

	virtual Hook::Reason process(struct Sample *smp)
	{
		timespec *ts;

		assert(state == State::STARTED);

		switch (mode) {
			case SHIFT_ORIGIN:
				ts = &smp->ts.origin;
				break;

			case SHIFT_RECEIVED:
				ts = &smp->ts.received;
				break;

			default:
				return Hook::Reason::ERROR;
		}

		*ts = time_add(ts, &offset);

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "shift_ts";
static char d[] = "Shift timestamps of samples";
static HookPlugin<ShiftTimestampHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */
