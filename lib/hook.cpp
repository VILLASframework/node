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

Hook::Hook(struct vpath *p, struct node *n, int fl, int prio, bool en) :
	state(State::INITIALIZED),
	flags(fl),
	priority(prio),
	enabled(en),
	path(p),
	node(n)
{
	int ret;

	logger = logging.get("hook");

	ret = signal_list_init(&signals);
	if (ret)
		throw RuntimeError("Failed to initialize signal list");

	/* We dont need to parse builtin hooks. */
	state = flags & (int) Hook::Flags::BUILTIN ? State::CHECKED : State::INITIALIZED;
}

Hook::~Hook()
{
	signal_list_destroy(&signals);
}

void Hook::prepare()
{
	assert(state == State::CHECKED);

	state = State::PREPARED;
}

void Hook::parse(json_t *c)
{
	int ret;
	json_error_t err;

	assert(state != State::STARTED);

	ret = json_unpack_ex(c, &err, 0, "{ s?: i, s?: b }",
		"priority", &priority,
		"enabled", &enabled
	);
	if (ret)
		throw ConfigError(c, err, "node-config-hook");

	cfg = c;

	state = State::PARSED;
}
