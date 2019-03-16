
/** Node direction
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

#include <villas/utils.h>
#include <villas/hook.h>
#include <villas/hook_list.h>
#include <villas/node.h>
#include <villas/node_direction.h>

int node_direction_prepare(struct node_direction *nd, struct node *n)
{
	assert(nd->state == STATE_CHECKED);

#ifdef WITH_HOOKS
	int ret;
	int t = nd->direction == NODE_DIR_OUT ? HOOK_NODE_WRITE : HOOK_NODE_READ;
	int m = nd->builtin ? t | HOOK_BUILTIN : 0;

	ret = hook_list_prepare(&nd->hooks, &nd->signals, m, NULL, n);
	if (ret)
		return ret;
#endif /* WITH_HOOKS */

	nd->state = STATE_PREPARED;

	return 0;
}

int node_direction_init(struct node_direction *nd, enum node_dir dir, struct node *n)
{
	int ret;

	assert(nd->state == STATE_DESTROYED);

	nd->direction = dir;
	nd->enabled = 1;
	nd->vectorize = 1;
	nd->builtin = 1;

	nd->hooks.state = STATE_DESTROYED;
	nd->signals.state = STATE_DESTROYED;

#ifdef WITH_HOOKS
	ret = hook_list_init(&nd->hooks);
	if (ret)
		return ret;
#endif /* WITH_HOOKS */

	ret = signal_list_init(&nd->signals);
	if (ret)
		return ret;

	nd->state = STATE_INITIALIZED;

	return 0;
}

int node_direction_destroy(struct node_direction *nd, struct node *n)
{
	int ret = 0;

	assert(nd->state != STATE_DESTROYED && nd->state != STATE_STARTED);

#ifdef WITH_HOOKS
	ret = hook_list_destroy(&nd->hooks);
	if (ret)
		return ret;
#endif /* WITH_HOOKS */

	ret = signal_list_destroy(&nd->signals);
	if (ret)
		return ret;

	nd->state = STATE_DESTROYED;

	return 0;
}

int node_direction_parse(struct node_direction *nd, struct node *n, json_t *cfg)
{
	int ret;

	assert(nd->state == STATE_INITIALIZED);

	json_error_t err;
	json_t *json_hooks = NULL;
	json_t *json_signals = NULL;

	nd->cfg = cfg;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: o, s?: o, s?: i, s?: b, s?: b }",
		"hooks", &json_hooks,
		"signals", &json_signals,
		"vectorize", &nd->vectorize,
		"builtin", &nd->builtin,
		"enabled", &nd->enabled
	);
	if (ret)
		jerror(&err, "Failed to parse node %s", node_name(n));

	if (n->_vt->flags & NODE_TYPE_PROVIDES_SIGNALS) {
		if (json_signals)
			error("Node %s does not support signal definitions", node_name(n));
	}
	else if (json_is_array(json_signals)) {
		ret = signal_list_parse(&nd->signals, json_signals);
		if (ret)
			error("Failed to parse signal definition of node %s", node_name(n));
	}
	else if (json_is_string(json_signals)) {
		const char *dt = json_string_value(json_signals);

		ret = signal_list_generate2(&nd->signals, dt);
		if (ret)
			return ret;
	}
	else {
		int count = 64;
		const char *type_str = "float";

		if (json_is_object(json_signals)) {
			json_unpack_ex(json_signals, &err, 0, "{ s: i, s: s }",
				"count", &count,
				"type", &type_str
			);
		}
		else
			warning("No signal definition found for node %s. Using the default config of 64 floating point signals.", node_name(n));

		int type = signal_type_from_str(type_str);
		if (type < 0)
			error("Invalid signal type %s", type_str);

		ret = signal_list_generate(&nd->signals, count, type);
		if (ret)
			return ret;
	}

#ifdef WITH_HOOKS
	if (json_hooks) {
		int m = nd->direction == NODE_DIR_OUT ? HOOK_NODE_WRITE : HOOK_NODE_READ;

		ret = hook_list_parse(&nd->hooks, json_hooks, m, NULL, n);
		if (ret < 0)
			return ret;
	}
#endif /* WITH_HOOKS */

	nd->state = STATE_PARSED;

	return 0;
}

int node_direction_check(struct node_direction *nd, struct node *n)
{
	assert(nd->state == STATE_PARSED);

	if (nd->vectorize <= 0)
		error("Invalid setting 'vectorize' with value %d for node %s. Must be natural number!", nd->vectorize, node_name(n));

	if (node_type(n)->vectorize && node_type(n)->vectorize < nd->vectorize)
		error("Invalid value for setting 'vectorize'. Node type requires a number smaller than %d!",
			node_type(n)->vectorize);

	nd->state = STATE_CHECKED;

	return 0;
}

int node_direction_start(struct node_direction *nd, struct node *n)
{
	assert(nd->state == STATE_PREPARED);

#ifdef WITH_HOOKS
	int ret;

	ret = hook_list_start(&nd->hooks);
	if (ret)
		return ret;
#endif /* WITH_HOOKS */

	nd->state = STATE_STARTED;

	return 0;
}

int node_direction_stop(struct node_direction *nd, struct node *n)
{
	assert(nd->state == STATE_STARTED);

#ifdef WITH_HOOKS
	int ret;

	ret = hook_list_stop(&nd->hooks);
	if (ret)
		return ret;
#endif /* WITH_HOOKS */

	nd->state = STATE_STOPPED;

	return 0;
}

struct vlist * node_direction_get_signals(struct node_direction *nd)
{
#ifdef WITH_HOOKS
	assert(nd->state == STATE_PREPARED);

	return hook_list_get_signals(&nd->hooks);
#else
	return &nd->signals;
#endif
}
