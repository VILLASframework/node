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

	int prio;
	int en;

	ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: b }",
		"priority", &prio,
		"enabled", &en
	);
	if (ret)
		throw ConfigError(json, err, "node-config-hook");

	if (prio < 0)
		throw ConfigError(json, "node-config-hook", "Priority must be equal or larger than zero");

	priority = prio;
	enabled = en;

	config = json;

	state = State::PARSED;
}
