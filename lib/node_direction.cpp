
/** Node direction
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

#include <villas/config.h>
#include <villas/utils.hpp>
#include <villas/hook.hpp>
#include <villas/hook_list.hpp>
#include <villas/node.h>
#include <villas/utils.hpp>
#include <villas/node_direction.h>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int node_direction_prepare(struct vnode_direction *nd, struct vnode *n)
{
	assert(nd->state == State::CHECKED);

#ifdef WITH_HOOKS
	int t = nd->direction == NodeDir::OUT ? (int) Hook::Flags::NODE_WRITE : (int) Hook::Flags::NODE_READ;
	int m = nd->builtin ? t | (int) Hook::Flags::BUILTIN : 0;

	hook_list_prepare(&nd->hooks, &nd->signals, m, nullptr, n);
#endif /* WITH_HOOKS */

	nd->state = State::PREPARED;

	return 0;
}

int node_direction_init(struct vnode_direction *nd, enum NodeDir dir, struct vnode *n)
{
	int ret;

	nd->direction = dir;
	nd->enabled = 1;
	nd->vectorize = 1;
	nd->builtin = 1;
	nd->path = nullptr;

#ifdef WITH_HOOKS
	ret = hook_list_init(&nd->hooks);
	if (ret)
		return ret;
#endif /* WITH_HOOKS */

	ret = signal_list_init(&nd->signals);
	if (ret)
		return ret;

	nd->state = State::INITIALIZED;

	return 0;
}

int node_direction_destroy(struct vnode_direction *nd, struct vnode *n)
{
	int ret = 0;

	assert(nd->state != State::DESTROYED && nd->state != State::STARTED);

#ifdef WITH_HOOKS
	ret = hook_list_destroy(&nd->hooks);
	if (ret)
		return ret;
#endif /* WITH_HOOKS */

	ret = signal_list_destroy(&nd->signals);
	if (ret)
		return ret;

	nd->state = State::DESTROYED;

	return 0;
}

int node_direction_parse(struct vnode_direction *nd, struct vnode *n, json_t *json)
{
	int ret;

	assert(nd->state == State::INITIALIZED);

	json_error_t err;
	json_t *json_hooks = nullptr;
	json_t *json_signals = nullptr;

	nd->config = json;

	ret = json_unpack_ex(json, &err, 0, "{ s?: o, s?: o, s?: i, s?: b, s?: b }",
		"hooks", &json_hooks,
		"signals", &json_signals,
		"vectorize", &nd->vectorize,
		"builtin", &nd->builtin,
		"enabled", &nd->enabled
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-in");

	if (n->_vt->flags & (int) NodeFlags::PROVIDES_SIGNALS) {
		/* Do nothing.. Node-type will provide signals */
	}
	else if (json_is_array(json_signals)) {
		ret = signal_list_parse(&nd->signals, json_signals);
		if (ret)
			throw ConfigError(json_signals, "node-config-node-signals", "Failed to parse signal definition");
	}
	else if (json_is_string(json_signals)) {
		const char *dt = json_string_value(json_signals);

		ret = signal_list_generate2(&nd->signals, dt);
		if (ret)
			return ret;
	}
	else {
		int count = DEFAULT_SAMPLE_LENGTH;
		const char *type_str = "float";

		if (json_is_object(json_signals)) {
			ret = json_unpack_ex(json_signals, &err, 0, "{ s: i, s: s }",
				"count", &count,
				"type", &type_str
			);
			if  (ret)
				throw ConfigError(json_signals, "node-config-node-signals", "Invalid signal definition");
		}

		enum SignalType type = signal_type_from_str(type_str);
		if (type == SignalType::INVALID)
			throw ConfigError(json_signals, "node-config-node-signals", "Invalid signal type {}", type_str);

		ret = signal_list_generate(&nd->signals, count, type);
		if (ret)
			return ret;
	}

#ifdef WITH_HOOKS
	if (json_hooks) {
		int m = nd->direction == NodeDir::OUT ? (int) Hook::Flags::NODE_WRITE : (int) Hook::Flags::NODE_READ;

		hook_list_parse(&nd->hooks, json_hooks, m, nullptr, n);
	}
#endif /* WITH_HOOKS */

	nd->state = State::PARSED;

	return 0;
}

void node_direction_check(struct vnode_direction *nd, struct vnode *n)
{
	assert(n->state != State::DESTROYED);

	if (nd->vectorize <= 0)
		throw RuntimeError("Invalid setting 'vectorize' with value {}. Must be natural number!", nd->vectorize);

#ifdef WITH_HOOKS
	hook_list_check(&nd->hooks);
#endif /* WITH_HOOKS */

	nd->state = State::CHECKED;
}

int node_direction_start(struct vnode_direction *nd, struct vnode *n)
{
	assert(nd->state == State::PREPARED);

#ifdef WITH_HOOKS
	hook_list_start(&nd->hooks);
#endif /* WITH_HOOKS */

	nd->state = State::STARTED;

	return 0;
}

int node_direction_stop(struct vnode_direction *nd, struct vnode *n)
{
	assert(nd->state == State::STARTED);

#ifdef WITH_HOOKS
	hook_list_stop(&nd->hooks);
#endif /* WITH_HOOKS */

	nd->state = State::STOPPED;

	return 0;
}

struct vlist * node_direction_get_signals(struct vnode_direction *nd)
{
	assert(nd->state == State::PREPARED);

#ifdef WITH_HOOKS
	if (vlist_length(&nd->hooks) > 0)
		return hook_list_get_signals(&nd->hooks);
#endif /* WITH_HOOKS */

	return &nd->signals;
}

unsigned node_direction_get_signals_max_cnt(struct vnode_direction *nd)
{
#ifdef WITH_HOOKS
	if (vlist_length(&nd->hooks) > 0)
		return MAX(vlist_length(&nd->signals), hook_list_get_signals_max_cnt(&nd->hooks));
#endif /* WITH_HOOKS */

	return vlist_length(&nd->signals);
}
