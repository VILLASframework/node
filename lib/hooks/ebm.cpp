/** Energy-based Metric hook.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <vector>

#include <villas/hook.hpp>
#include <villas/sample.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

class EBMHook : public Hook {

protected:
	std::vector<std::pair<int, int>> phases;

	double energy;

	struct Sample *last;

public:
	using Hook::Hook;

	virtual void parse(json_t *json)
	{
		int ret;

		size_t i;
		json_error_t err;
		json_t *json_phases, *json_phase;

		Hook::parse(json);

		ret = json_unpack_ex(json, &err, 0, "{ s: o }",
			"phases", &json_phases
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-ebm");

		if (!json_is_array(json_phases))
			throw ConfigError(json_phases, "node-config-hook-ebm-phases");

		json_array_foreach(json_phases, i, json_phase) {
			int voltage, current;

			ret = json_unpack_ex(json_phase, &err, 0, "[ i, i ]",
				&voltage, &current
			);
			if (ret)
				throw ConfigError(json, err, "node-config-hook-ebm-phases");

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

		logger->info("Energy: {}", energy);
	}

	virtual Hook::Reason process(struct Sample *smp)
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

/* Register hook */
static char n[] = "ebm";
static char d[] = "Energy-based Metric";
static HookPlugin<EBMHook, n, d, (int) Hook::Flags::PATH | (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE> p;

} /* namespace node */
} /* namespace villas */
