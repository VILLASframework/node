/** Gate hook.
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

#include <cmath>
#include <string>
#include <limits>

#include <villas/hook.hpp>
#include <villas/node.h>
#include <villas/sample.h>
#include <villas/timing.h>

namespace villas {
namespace node {

class GateHook : public Hook {

protected:
	enum class Mode {
		ABOVE,
		BELOW,
		RISING_EDGE,
		FALLING_EDGE
	} mode;

	std::string signalName;
	int signalIndex;
	double threshold;
	double duration;
	int samples;
	double previousValue;

	bool active;
	uint64_t startSequence;
	timespec startTime;

public:
	GateHook(struct vpath *p, struct node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		mode(Mode::RISING_EDGE),
		threshold(0.5),
		duration(-1),
		samples(-1),
		previousValue(std::numeric_limits<double>::quiet_NaN()),
		active(false)
	{ }

	virtual void parse(json_t *cfg)
	{
		int ret;

		json_error_t err;
		json_t *json_signal;

		const char *mode_str;

		assert(state != State::STARTED);

		ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s?: F, s?: F, s?: i, s?: s }",
			"signal", &json_signal,
			"threshold", &threshold,
			"duration", &duration,
			"samples", &samples,
			"mode", &mode_str
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-gate");

		if (mode_str) {
			if      (!strcmp(mode_str, "above"))
				mode = Mode::ABOVE;
			else if (!strcmp(mode_str, "below"))
				mode = Mode::BELOW;
			else if (!strcmp(mode_str, "rising_edge"))
				mode = Mode::RISING_EDGE;
			else if (!strcmp(mode_str, "falling_edge"))
				mode = Mode::FALLING_EDGE;
		}

		switch (json_typeof(json_signal)) {
			case JSON_STRING:
				signalName = json_string_value(json_signal);
				break;

			case JSON_INTEGER:
				signalName.clear();
				signalIndex = json_integer_value(json_signal);
				break;

			default:
				throw ConfigError(json_signal, "node-config-hook-cast-signals", "Invalid value for setting 'signal'");
		}

		state = State::PARSED;
	}

	virtual void prepare()
	{
		assert(state == State::CHECKED);

		if (!signalName.empty()) {
			signalIndex = vlist_lookup_index<struct signal>(&signals, signalName);
			if (signalIndex < 0)
				throw RuntimeError("Failed to find signal: {}", signalName);
		}

		/* Check if signal type is float */
		auto sig = (struct signal *) vlist_at(&signals, signalIndex);
		if (!sig)
			throw RuntimeError("Invalid signal index: {}", signalIndex);

		if (sig->type != SignalType::FLOAT)
			throw RuntimeError("Gate signal must be of type float");

		state = State::PREPARED;
	}


	virtual Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);

		Hook::Reason reason;
		double value = smp->data[signalIndex].f;

		if (active) {
			if (duration > 0 && time_delta(&smp->ts.origin, &startTime) < duration)
				reason = Reason::OK;
			else if (samples > 0 && smp->sequence - startSequence < (uint64_t) samples)
				reason = Reason::OK;
			else {
				reason = Reason::SKIP_SAMPLE;
				active = false;
			}
		}

		if (!active) {
			switch (mode) {
				case Mode::ABOVE:
					reason = value > threshold ? Reason::OK : Reason::SKIP_SAMPLE;
					break;

				case Mode::BELOW:
					reason = value < threshold ? Reason::OK : Reason::SKIP_SAMPLE;
					break;

				case Mode::RISING_EDGE:
					reason = (!std::isnan(previousValue) && value > previousValue) ? Reason::OK : Reason::SKIP_SAMPLE;
					break;

				case Mode::FALLING_EDGE:
					reason = (!std::isnan(previousValue) && value < previousValue) ? Reason::OK : Reason::SKIP_SAMPLE;
					break;

				default:
					reason = Reason::ERROR;
			}

			if (reason == Reason::OK) {
				startTime = smp->ts.origin;
				startSequence = smp->sequence;
				active = true;
			}
		}

		previousValue = value;

		return reason;
	}
};

/* Register hook */
static char n[] = "gate";
static char d[] = "Skip samples only if an enable signal is under a specified threshold";
static HookPlugin<GateHook, n, d, (int) Hook::Flags::NODE_READ | (int) Hook::Flags::PATH> p;

} /* namespace node */
} /* namespace villas */

/** @} */
