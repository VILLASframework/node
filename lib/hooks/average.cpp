/** Average hook.
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

#include <string.h>

#include <villas/hook.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/signal.h>
#include <villas/sample.h>
#include <villas/bitset.h>

namespace villas {
namespace node {

class AverageHook : public Hook {

protected:
	int offset;

	bitset mask;
	vlist signal_names;

public:
	AverageHook(struct path *p, struct node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en)
	{
		int ret;

		ret = vlist_init(&signal_names);
		if (ret)
			throw RuntimeError("Failed to intialize list");

		ret = bitset_init(&mask, 128);
		if (ret)
			throw RuntimeError("Failed to intialize bitset");

		bitset_clear_all(&mask);

		state = STATE_INITIALIZED;
	}

	virtual ~AverageHook()
	{
		vlist_destroy(&signal_names, NULL, true);

		bitset_destroy(&mask);
	}

	virtual void prepare()
	{
		int ret;
		struct signal *avg_sig;

		assert(state == STATE_CHECKED);

		/* Setup mask */
		for (size_t i = 0; i < vlist_length(&signal_names); i++) {
			char *signal_name = (char *) vlist_at_safe(&signal_names, i);

			int index = vlist_lookup_index(&signals, signal_name);
			if (index < 0)
				throw RuntimeError("Failed to find signal {}", signal_name);

			bitset_set(&mask, index);
		}

		if (!bitset_get_value(&mask))
			throw RuntimeError("Invalid signal mask");

		/* Add averaged signal */
		avg_sig = signal_create("average", NULL, SIGNAL_TYPE_FLOAT);
		if (!avg_sig)
			throw RuntimeError("Failed to create new signal");

		ret = vlist_insert(&signals, offset, avg_sig);
		if (ret)
			throw RuntimeError("Failed to intialize list");

		state = STATE_PREPARED;
	}

	virtual void parse(json_t *cfg)
	{
		int ret;
		size_t i;
		json_error_t err;
		json_t *json_signals, *json_signal;

		assert(state != STATE_STARTED);

		Hook::parse(cfg);

		ret = json_unpack_ex(cfg, &err, 0, "{ s: i, s: o }",
			"offset", &offset,
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
					bitset_set(&mask, json_integer_value(json_signal));
					break;

				default:
					throw ConfigError(json_signal, "node-config-hook-average-signals", "Invalid value for setting 'signals'");
			}
		}

		state = STATE_PARSED;
	}

	virtual int process(sample *smp)
	{
		double avg, sum = 0;
		int n = 0;

		assert(state == STATE_STARTED);

		for (unsigned k = 0; k < smp->length; k++) {
			if (!bitset_test(&mask, k))
				continue;

			switch (sample_format(smp, k)) {
				case SIGNAL_TYPE_INTEGER:
					sum += smp->data[k].i;
					break;

				case SIGNAL_TYPE_FLOAT:
					sum += smp->data[k].f;
					break;

				case SIGNAL_TYPE_INVALID:
				case SIGNAL_TYPE_COMPLEX:
				case SIGNAL_TYPE_BOOLEAN:
					return HOOK_ERROR; /* not supported */
			}

			n++;
		}

		avg = sum / n;
		sample_data_insert(smp, (union signal_data *) &avg, offset, 1);
		smp->signals = &signals;

		return HOOK_OK;
	}
};

/* Register hook */
static HookPlugin<AverageHook> p(
	"average",
	"Calculate average over some signals",
	HOOK_PATH | HOOK_NODE_READ | HOOK_NODE_WRITE,
	99
);

} /* namespace node */
} /* namespace villas */

/** @} */
