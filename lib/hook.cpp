/** Hook-releated functions.
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

#include <cstring>
#include <cmath>

#include <villas/timing.h>
#include <villas/node/config.h>
#include <villas/hook.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/path.h>
#include <villas/utils.hpp>
#include <villas/node.h>

const char *hook_reasons[] = {
	"ok", "error", "skip-sample", "stop-processing"
};

using namespace villas;
using namespace villas::node;

Hook::Hook(struct vpath *p, struct vnode *n, int fl, int prio, bool en) :
	state(State::INITIALIZED),
	flags(fl),
	priority(prio),
	enabled(en),
	path(p),
	node(n),
	config(nullptr)
{
	int ret;

	ret = signal_list_init(&signals);
	if (ret)
		throw RuntimeError("Failed to initialize signal list");

	/* We dont need to parse builtin hooks. */
	state = flags & (int) Hook::Flags::BUILTIN ? State::CHECKED : State::INITIALIZED;
}

Hook::~Hook()
{
	int ret __attribute__((unused));

	ret = signal_list_destroy(&signals);
}

void Hook::prepare(struct vlist *sigs)
{
	int ret;

	assert(state == State::CHECKED);

	ret = signal_list_copy(&signals, sigs);
	if (ret)
		throw RuntimeError("Failed to copy signal list");

	prepare();

	state = State::PREPARED;
}

void Hook::parse(json_t *json)
{
	int ret;
	json_error_t err;

	assert(state != State::STARTED);

	int prio = -1;
	int en = -1;

	ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: b }",
		"priority", &prio,
		"enabled", &en
	);
	if (ret)
		throw ConfigError(json, err, "node-config-hook");

	if (prio >= 0)
		priority = prio;

	if (en >= 0)
		enabled = en;

	config = json;

	state = State::PARSED;
}

void SingleSignalHook::parse(json_t *json)
{
	int ret;

	json_error_t err;
	json_t *json_signal;

	Hook::parse(json);

	ret = json_unpack_ex(json, &err, 0, "{ s: o }",
		"signal", &json_signal
	);
	if (ret)
		throw ConfigError(json, err, "node-config-hook");

	if (!json_is_string(json_signal))
		throw ConfigError(json_signal, "node-config-hook-signals", "Invalid value for setting 'signals'");

	signalName = json_string_value(json_signal);
}


void SingleSignalHook::prepare()
{
	Hook::prepare();

	/* Setup mask */
	int index = vlist_lookup_index<struct signal>(&signals, signalName);
	if (index < 0)
		throw RuntimeError("Failed to find signal {}", signalName);

	signalIndex = (unsigned) index;
}

/* Multi Signal Hook */

void MultiSignalHook::parse(json_t *json)
{
	int ret;
	size_t i;

	json_error_t err;
	json_t *json_signals = nullptr;
	json_t *json_signal = nullptr;

	Hook::parse(json);

	ret = json_unpack_ex(json, &err, 0, "{ s?: o, s?: o }",
		"signals", &json_signals,
		"signal", &json_signal
	);
	if (ret)
		throw ConfigError(json, err, "node-config-hook");

	if (json_signals) {
		if (!json_is_array(json_signals))
			throw ConfigError(json_signals, "node-config-hook-signals", "Setting 'signals' must be a list of signal names");

		json_array_foreach(json_signals, i, json_signal) {
			if (!json_is_string(json_signal))
				throw ConfigError(json_signal, "node-config-hook-signals", "Invalid value for setting 'signals'");

			const char *name = json_string_value(json_signal);

			signalNames.push_back(name);
		}
	}
	else if (json_signal) {
		if (!json_is_string(json_signal))
			throw ConfigError(json_signal, "node-config-hook-signals", "Invalid value for setting 'signals'");

		const char *name = json_string_value(json_signal);

		signalNames.push_back(name);
	}
	else
		throw ConfigError(json, "node-config-hook-signals", "Missing 'signals' setting");
}


void MultiSignalHook::prepare()
{
	Hook::prepare();

	for (const auto &signalName : signalNames) {
		int index = vlist_lookup_index<struct signal>(&signals, signalName);
		if (index < 0)
			throw RuntimeError("Failed to find signal {}", signalName);

		signalIndices.push_back(index);
	}
}

void MultiSignalHook::check()
{
	Hook::check();

	if (signalNames.size() == 0)
		throw RuntimeError("At least a single signal must be provided");
}
