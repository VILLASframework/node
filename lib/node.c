/** Nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <string.h>

#include <villas/config.h>
#include <villas/hook.h>
#include <villas/sample.h>
#include <villas/node.h>
#include <villas/utils.h>
#include <villas/plugin.h>
#include <villas/config_helper.h>
#include <villas/mapping.h>
#include <villas/timing.h>
#include <villas/signal.h>
#include <villas/memory.h>

static int node_direction_init(struct node_direction *nd, struct node *n)
{
	nd->enabled = 0;
	nd->vectorize = 1;
	nd->builtin = 1;

	list_init(&nd->signals);

#ifdef WITH_HOOKS
	/* Add internal hooks if they are not already in the list */
	list_init(&nd->hooks);

	if (nd->builtin) {
		int ret;
		for (size_t i = 0; i < list_length(&plugins); i++) {
			struct plugin *q = (struct plugin *) list_at(&plugins, i);

			if (q->type != PLUGIN_TYPE_HOOK)
				continue;

			struct hook_type *vt = &q->hook;

			if (!(vt->flags & HOOK_NODE) || !(vt->flags & HOOK_BUILTIN))
				continue;

			struct hook *h = (struct hook *) alloc(sizeof(struct hook));

			ret = hook_init(h, vt, NULL, n);
			if (ret)
				return ret;

			list_push(&nd->hooks, h);
		}
	}
#endif /* WITH_HOOKS */

	return 0;
}

static int node_direction_destroy(struct node_direction *nd, struct node *n)
{
	int ret;

	ret = list_destroy(&nd->signals, (dtor_cb_t) signal_destroy, true);
	if (ret)
		return ret;

#ifdef WITH_HOOKS
	ret = list_destroy(&nd->hooks, (dtor_cb_t) hook_destroy, true);
	if (ret)
		return ret;
#endif

	return 0;
}

static int node_direction_parse(struct node_direction *nd, struct node *n, json_t *cfg)
{
	int ret;

	json_error_t err;
	json_t *json_hooks = NULL;
	json_t *json_signals = NULL;

	nd->cfg = cfg;
	nd->enabled = 1;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: o, s?: o, s?: i, s?: b, s?: b }",
		"hooks", &json_hooks,
		"signals", &json_signals,
		"vectorize", &nd->vectorize,
		"builtin", &nd->builtin,
		"enabled", &nd->enabled
	);
	if (ret)
		jerror(&err, "Failed to parse node '%s'", node_name(n));

#ifdef WITH_HOOKS
	if (json_hooks) {
		ret = hook_parse_list(&nd->hooks, json_hooks, NULL, n);
		if (ret < 0)
			return ret;
	}
#endif /* WITH_HOOKS */

	if (json_signals) {
		ret = signal_parse_list(&nd->signals, json_signals);
		if (ret)
			error("Failed to parse signal definition of node '%s'", node_name(n));
	}

	return 0;
}

static int node_direction_check(struct node_direction *nd, struct node *n)
{
	if (nd->vectorize <= 0)
		error("Invalid `vectorize` value %d for node %s. Must be natural number!", nd->vectorize, node_name(n));

	if (n->_vt->vectorize && n->_vt->vectorize < nd->vectorize)
		error("Invalid value for `vectorize`. Node type requires a number smaller than %d!",
			n->_vt->vectorize);

	return 0;
}

static int node_direction_start(struct node_direction *nd, struct node *n)
{
#ifdef WITH_HOOKS
	int ret;

	/* We sort the hooks according to their priority before starting the path */
	list_sort(&nd->hooks, hook_cmp_priority);

	for (size_t i = 0; i < list_length(&nd->hooks); i++) {
		struct hook *h = (struct hook *) list_at(&nd->hooks, i);

		ret = hook_start(h);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	return 0;
}

static int node_direction_stop(struct node_direction *nd, struct node *n)
{
#ifdef WITH_HOOKS
	int ret;

	for (size_t i = 0; i < list_length(&nd->hooks); i++) {
		struct hook *h = (struct hook *) list_at(&nd->hooks, i);

		ret = hook_stop(h);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	return 0;
}

int node_init(struct node *n, struct node_type *vt)
{
	int ret;
	assert(n->state == STATE_DESTROYED);

	n->_vt = vt;
	n->_vd = alloc(vt->size);

	n->stats = NULL;
	n->name = NULL;
	n->_name = NULL;
	n->_name_long = NULL;

	/* Default values */
	n->samplelen = DEFAULT_SAMPLELEN;

	ret = node_direction_init(&n->in, n);
	if (ret)
		return ret;

	ret = node_direction_init(&n->out, n);
	if (ret)
		return ret;

	n->state = STATE_INITIALIZED;

	list_push(&vt->instances, n);

	return 0;
}

int node_parse(struct node *n, json_t *json, const char *name)
{
	struct node_type *nt;
	int ret;

	json_error_t err;

	const char *type;

	n->name = strdup(name);

	ret = json_unpack_ex(json, &err, 0, "{ s: s, s?: i }",
		"type", &type,
		"samplelen", &n->samplelen
	);
	if (ret)
		jerror(&err, "Failed to parse node '%s'", node_name(n));

	nt = node_type_lookup(type);
	assert(nt == n->_vt);

	struct {
		const char *str;
		struct node_direction *dir;
	} dirs[] = {
		{ "in", &n->in },
		{ "out", &n->out }
	};

	const char *fields[] = { "builtin", "vectorize", "signals", "hooks" };

	for (int j = 0; j < ARRAY_LEN(dirs); j++) {
		json_t *json_dir = json_object_get(json, dirs	[j].str);

		// Skip if direction is unused
		if (!json_dir)
			json_dir = json_pack("{ s: b }", "enabled", 0);

		// Copy missing fields from main node config to direction config
		for (int i = 0; i < ARRAY_LEN(fields); i++) {
			json_t *json_field_dir  = json_object_get(json_dir, fields[i]);
			json_t *json_field_node = json_object_get(json, fields[i]);

			if (json_field_node && !json_field_dir)
				json_object_set(json_dir, fields[i], json_field_node);
		}

		ret = node_direction_parse(dirs[j].dir, n, json_dir);
		if (ret)
			error("Failed to parse %s direction of node '%s'", dirs[j].str, node_name(n));
	}

	ret = n->_vt->parse ? n->_vt->parse(n, json) : 0;
	if (ret)
		error("Failed to parse node '%s'", node_name(n));

	n->cfg = json;
	n->state = STATE_PARSED;

	return ret;
}

int node_parse_cli(struct node *n, int argc, char *argv[])
{
	int ret;

	assert(n->_vt);

	if (n->_vt->parse_cli) {
		n->name = strdup("cli");

		ret = n->_vt->parse_cli(n, argc, argv);
		if (ret)
			return ret;

		n->state = STATE_PARSED;
	}
	else {
		n->cfg = json_load_cli(argc, argv);
		if (!n->cfg)
			return -1;

		ret = node_parse(n, n->cfg, "cli");
	}

	return ret;
}

int node_check(struct node *n)
{
	int ret;
	assert(n->state != STATE_DESTROYED);

	ret = node_direction_check(&n->in, n);
	if (ret)
		return ret;

	ret = node_direction_check(&n->out, n);
	if (ret)
		return ret;

	ret = n->_vt->check ? n->_vt->check(n) : 0;
	if (ret)
		return ret;

	n->state = STATE_CHECKED;

	return 0;
}

int node_start(struct node *n)
{
	int ret;

	assert(n->state == STATE_CHECKED);
	assert(n->_vt->state == STATE_STARTED);

	info("Starting node %s", node_name_long(n));
	{ INDENT
		ret = node_direction_start(&n->in, n);
		if (ret)
			return ret;

		ret = node_direction_start(&n->out, n);
		if (ret)
			return ret;

		ret = n->_vt->start ? n->_vt->start(n) : 0;
		if (ret)
			return ret;
	}

	n->state = STATE_STARTED;

	n->sequence = 0;

	return ret;
}

int node_stop(struct node *n)
{
	int ret;

	if (n->state != STATE_STARTED && n->state != STATE_CONNECTED)
		return 0;

	info("Stopping node %s", node_name(n));
	{ INDENT
		ret = node_direction_stop(&n->in, n);
		if (ret)
			return ret;

		ret = node_direction_stop(&n->out, n);
		if (ret)
			return ret;

		ret = n->_vt->stop ? n->_vt->stop(n) : 0;
	}

	if (ret == 0)
		n->state = STATE_STOPPED;

	return ret;
}

int node_destroy(struct node *n)
{
	int ret;
	assert(n->state != STATE_DESTROYED && n->state != STATE_STARTED);

	ret = node_direction_destroy(&n->in, n);
	if (ret)
		return ret;

	ret = node_direction_destroy(&n->out, n);
	if (ret)
		return ret;

	if (n->_vt->destroy){
		ret = (int) n->_vt->destroy(n);
		if(ret){
			return ret;
		}
	}

	list_remove(&n->_vt->instances, n);

	if (n->_vd)
		free(n->_vd);

	if (n->_name)
		free(n->_name);

	if (n->_name_long)
		free(n->_name_long);

	if (n->name)
		free(n->name);

	n->state = STATE_DESTROYED;

	return 0;
}

int node_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int readd, nread = 0;

	if (!n->_vt->read)
		return -1;

	/* Send in parts if vector not supported */
	if (n->_vt->vectorize > 0 && n->_vt->vectorize < cnt) {
		while (cnt - nread > 0) {
			readd = n->_vt->read(n, &smps[nread], MIN(cnt - nread, n->_vt->vectorize), release);
			if (readd < 0)
				return readd;

			nread += readd;
		}
	}
	else {
		nread = n->_vt->read(n, smps, cnt, release);
		if (nread < 0)
			return nread;
	}

	/* Add missing fields */
	for (int i = 0; i < nread; i++) {
		smps[i]->source = n;

		if (!(smps[i]->flags & SAMPLE_HAS_SEQUENCE))
			smps[i]->sequence = n->sequence++;

		if (!(smps[i]->flags & SAMPLE_HAS_ORIGIN) ||
		    !(smps[i]->flags & SAMPLE_HAS_RECEIVED)) {

			struct timespec now = time_now();

			if (!(smps[i]->flags & SAMPLE_HAS_RECEIVED))
				smps[i]->ts.received = now;

			if (!(smps[i]->flags & SAMPLE_HAS_ORIGIN))
				smps[i]->ts.origin = now;
		}
	}

#ifdef WITH_HOOKS
	/* Run read hooks */
	int rread = hook_read_list(&n->in.hooks, smps, nread);
	int skipped = nread - rread;

	if (skipped > 0 && n->stats != NULL) {
		stats_update(n->stats, STATS_SKIPPED, skipped);
	}

	debug(LOG_NODES | 5, "Received %u samples from node %s of which %d have been skipped", nread, node_name(n), skipped);

	return rread;
#else
	debug(LOG_NODES | 5, "Received %u samples from node %s", nread, node_name(n));

	return nread;
#endif /* WITH_HOOKS */
}

int node_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int sent, nsent = 0;

	if (!n->_vt->write)
		return -1;

#ifdef WITH_HOOKS
	/* Run write hooks */
	cnt = hook_write_list(&n->out.hooks, smps, cnt);
	if (cnt <= 0)
		return cnt;
#endif /* WITH_HOOKS */

	/* Send in parts if vector not supported */
	if (n->_vt->vectorize > 0 && n->_vt->vectorize < cnt) {
		while (cnt - nsent > 0) {
			sent = n->_vt->write(n, &smps[nsent], MIN(cnt - nsent, n->_vt->vectorize), release);
			if (sent < 0)
				return sent;

			nsent += sent;
			debug(LOG_NODES | 5, "Sent %u samples to node %s", sent, node_name(n));
		}
	}
	else {
		nsent = n->_vt->write(n, smps, cnt, release);
		if (nsent < 0)
			return nsent;

		debug(LOG_NODES | 5, "Sent %u samples to node %s", nsent, node_name(n));
	}

	return nsent;
}

char * node_name(struct node *n)
{
	if (!n->_name)
		strcatf(&n->_name, CLR_RED("%s") "(" CLR_YEL("%s") ")", n->name, plugin_name(n->_vt));

	return n->_name;
}

char * node_name_long(struct node *n)
{
	if (!n->_name_long) {
		if (n->_vt->print) {
			struct node_type *vt = n->_vt;
			char *name_long = vt->print(n);
			strcatf(&n->_name_long, "%s: #in.hooks=%zu, in.vectorize=%d, #out.hooks=%zu, out.vectorize=%d, samplelen=%d, %s",
				node_name(n),
				list_length(&n->in.hooks), n->in.vectorize,
				list_length(&n->out.hooks), n->out.vectorize,
				n->samplelen, name_long);

			free(name_long);
		}
		else
			n->_name_long = node_name(n);
	}

	return n->_name_long;
}

const char * node_name_short(struct node *n)
{
	return n->name;
}

int node_reverse(struct node *n)
{
	return n->_vt->reverse ? n->_vt->reverse(n) : -1;
}

int node_fd(struct node *n)
{
	return n->_vt->fd ? n->_vt->fd(n) : -1;
}

struct memory_type * node_memory_type(struct node *n, struct memory_type *parent)
{
	return n->_vt->memory_type ? n->_vt->memory_type(n, parent) : &memory_hugepage;
}

int node_parse_list(struct list *list, json_t *cfg, struct list *all)
{
	struct node *node;
	const char *str;
	char *allstr = NULL;

	size_t index;
	json_t *elm;

	switch (json_typeof(cfg)) {
		case JSON_STRING:
			str = json_string_value(cfg);
			node = list_lookup(all, str);
			if (!node)
				goto invalid2;

			list_push(list, node);
			break;

		case JSON_ARRAY:
			json_array_foreach(cfg, index, elm) {
				if (!json_is_string(elm))
					goto invalid;

				node = list_lookup(all, json_string_value(elm));
				if (!node)
					goto invalid;

				list_push(list, node);
			}
			break;

		default:
			goto invalid;
	}

	return 0;

invalid:
	error("The node list must be an a single or an array of strings referring to the keys of the 'nodes' section");

	return -1;

invalid2:
	for (size_t i = 0; i < list_length(all); i++) {
		struct node *n = (struct node *) list_at(all, i);

		strcatf(&allstr, " %s", node_name_short(n));
	}

	error("Unknown node '%s'. Choose of one of: %s", str, allstr);

	return 0;
}
