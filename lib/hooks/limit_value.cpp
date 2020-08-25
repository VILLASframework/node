/** Limit hook.
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

#include <bitset>

#include <cstring>

#include <villas/hook.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/signal.h>
#include <villas/sample.h>

namespace villas {
namespace node {

class LimitValueHook : public Hook {

protected:
	unsigned offset;

	float min, max;

	std::bitset<MAX_SAMPLE_LENGTH> mask;
	vlist signal_names;

public:
	LimitValueHook(struct vpath *p, struct node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en)
	{
		int ret;

		ret = vlist_init(&signal_names);
		if (ret)
			throw RuntimeError("Failed to intialize list");

		state = State::INITIALIZED;
	}

	virtual ~LimitValueHook()
	{
		vlist_destroy(&signal_names, nullptr, true);
	}

	virtual void prepare()
	{
		int ret;
		struct signal *avg_sig;

		assert(state == State::CHECKED);

		/* Setup mask */
		for (size_t i = 0; i < vlist_length(&signal_names); i++) {
			char *signal_name = (char *) vlist_at_safe(&signal_names, i);

			int index = vlist_lookup_index<struct signal>(&signals, signal_name);
			if (index < 0)
				throw RuntimeError("Failed to find signal {}", signal_name);

			mask.set(index);
		}

		if (mask.none())
			throw RuntimeError("Invalid signal mask");

		/* Add averaged signal */
		avg_sig = signal_create("average", nullptr, SignalType::FLOAT);
		if (!avg_sig)
			throw RuntimeError("Failed to create new signal");

		ret = vlist_insert(&signals, offset, avg_sig);
		if (ret)
			throw RuntimeError("Failed to intialize list");

		state = State::PREPARED;
	}

	virtual void parse(json_t *cfg)
	{
		int ret;
		size_t i;
		json_error_t err;
		json_t *json_signals, *json_signal;

		assert(state != State::STARTED);

		Hook::parse(cfg);

		ret = json_unpack_ex(cfg, &err, 0, "{ s: f, s: f, s: o }",
			"min", &min,
			"min", &max,
			"signals", &json_signals
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-average");

		if (!json_is_array(json_signals))
			throw ConfigError(json_signals, "node-config-hook-average-signals", "Setting 'signals' must be a list of signal names");

		json_array_foreach(json_signals, i, json_signal) {
			switch (json_typeof(json_signal)) {
				case JSON_STRING:
					vlist_push(&signal_names, strdup(json_string_value(json_signal)));
					break;

				case JSON_INTEGER:
					mask.set(json_integer_value(json_signal));
					break;

				default:
					throw ConfigError(json_signal, "node-config-hook-average-signals", "Invalid value for setting 'signals'");
			}
		}

		state = State::PARSED;
	}

	virtual Hook::Reason process(sample *smp)
	{
		assert(state == State::STARTED);

		for (unsigned k = 0; k < smp->length; k++) {
			if (!mask.test(k))
				continue;

			switch (sample_format(smp, k)) {
				case SignalType::INTEGER:
					if (smp->data[k].i > max)
						smp->data[k].i = max;

					if (smp->data[k].i < min)
						smp->data[k].i = min;
					break;

				case SignalType::FLOAT:
					if (smp->data[k].f > max)
						smp->data[k].f = max;

					if (smp->data[k].f < min)
						smp->data[k].f = min;
					break;

				case SignalType::INVALID:
				case SignalType::COMPLEX:
				case SignalType::BOOLEAN:
					return Hook::Reason::ERROR; /* not supported */
			}
		}

		return Reason::OK;
	}
};

/* Register hook */
static char n[] = "average";
static char d[] = "Calculate average over some signals";
static HookPlugin<LimitValueHook, n , d, (int) Hook::Flags::PATH | (int) Hook::Flags::NODE_READ | (int) Hook::Flags::NODE_WRITE> p;

} /* namespace node */
} /* namespace villas */

/** @} */
