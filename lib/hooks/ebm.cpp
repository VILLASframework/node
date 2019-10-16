/** Energy-based Metric hook.
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

#include <vector>

#include <villas/hook.hpp>
#include <villas/log.h>
#include <villas/sample.h>
#include <villas/timing.h>

namespace villas {
namespace node {

class EBMHook : public Hook {

protected:
	std::vector<std::pair<int, int>> phases;

	double energy;

	sample *last;

public:
	using Hook::Hook;

	virtual void parse(json_t *cfg)
	{
		int ret;

		size_t i;
		json_error_t err;
		json_t *json_phases, *json_phase;

		ret = json_unpack_ex(cfg, &err, 0, "{ s: o }",
			"phases", &json_phases
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-ebm");

		if (!json_is_array(json_phases))
			throw ConfigError(json_phases, "node-config-hook-ebm-phases");

		json_array_foreach(json_phases, i, json_phase) {
			int voltage, current;

			ret = json_unpack_ex(json_phase, &err, 0, "[ i, i ]",
				&voltage, &current
			);
			if (ret)
				throw ConfigError(cfg, err, "node-config-hook-ebm-phases");

			phases.emplace_back(voltage, current);
		}

		state = State::PARSED;
	}

	virtual void start()
	{
		assert(state == State::PREPARED);

		energy = 0;
		last = nullptr;

		state = State::STARTED;
	}

	virtual void periodic()
	{
		assert(state == State::STARTED);

		info("Energy: %f", energy);
	}

	virtual Hook::Reason process(sample *smp)
	{
		double P, P_last, dt;

		assert(state == State::STARTED);

		if (last) {
			for (auto phase : phases) {
				/* Trapazoidal rule */
				dt = time_delta(&last->ts.origin, &smp->ts.origin);

				P      =  smp->data[phase.first].f *  smp->data[phase.second].f;
				P_last = last->data[phase.first].f * last->data[phase.second].f;

				energy += dt * (P_last + P) / 2.0;
			}

			sample_decref(last);
		}

		sample_incref(smp);
		last = smp;

		return Reason::OK;
	}
};

static HookPlugin<EBMHook> p(
	"ebm",
	"Energy-based Metric",
	(int) Hook::Flags::PATH | (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE,
	99
);

} /* namespace node */
} /* namespace villas */

/** @} */
