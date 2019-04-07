/** Scale hook.
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
#include <villas/sample.h>

namespace villas {
namespace node {

class ScaleHook : public Hook {

protected:
	char *signal_name;
	unsigned signal_index;

	double scale;
	double offset;

public:
	ScaleHook(struct path *p, struct node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		scale(1),
		offset(0)
	{ }

	virtual void prepare()
	{
		assert(state != STATE_STARTED);

		if (signal_name) {
			signal_index = vlist_lookup_index(&signals, signal_name);
			if (signal_index < 0)
				throw RuntimeError("Failed to find signal: {}", signal_name);
		}

		state = STATE_PREPARED;
	}

	virtual ~ScaleHook()
	{
		if (signal_name)
			free(signal_name);
	}

	virtual void parse(json_t *cfg)
	{
		int ret;
		json_t *json_signal;
		json_error_t err;

		assert(state != STATE_STARTED);

		ret = json_unpack_ex(cfg, &err, 0, "{ s?: F, s?: F, s: o }",
			"scale", &scale,
			"offset", &offset,
			"signal", &json_signal
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-scale");

		switch (json_typeof(json_signal)) {
			case JSON_STRING:
				signal_name = strdup(json_string_value(json_signal));
				break;

			case JSON_INTEGER:
				signal_name = nullptr;
				signal_index = json_integer_value(json_signal);
				break;

			default:
				throw ConfigError(json_signal, "node-config-hook-scale-signal", "Invalid value for setting 'signal'");
		}

		state = STATE_PARSED;
	}

	virtual int process(sample *smp)
	{
		int k = signal_index;

		assert(state == STATE_STARTED);

		switch (sample_format(smp, k)) {
			case SIGNAL_TYPE_INTEGER:
				smp->data[k].i = smp->data[k].i * scale + offset;
				break;

			case SIGNAL_TYPE_FLOAT:
				smp->data[k].f = smp->data[k].f * scale + offset;
				break;

			case SIGNAL_TYPE_COMPLEX:
				smp->data[k].z = smp->data[k].z * scale + offset;
				break;

			case SIGNAL_TYPE_BOOLEAN:
				smp->data[k].b = smp->data[k].b * scale + offset;
				break;

			default: { }
		}

		return HOOK_OK;
	}
};

/* Register hook */
static HookPlugin<ScaleHook> p(
	"scale",
	"Scale signals by a factor and add offset",
	HOOK_PATH | HOOK_NODE_READ | HOOK_NODE_WRITE,
	99
);

} /* namespace node */
} /* namespace villas */

/** @} */
