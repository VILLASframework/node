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
#include <villas/sample.h>
#include <villas/node.h>
#include <villas/utils.h>
#include <villas/config.h>
#include <villas/plugin.h>
#include <villas/config_helper.h>
#include <villas/mapping.h>
#include <villas/timing.h>

int node_init(struct node *n, struct node_type *vt)
{
	static int max_id;

	assert(n->state == STATE_DESTROYED);

	n->_vt = vt;
	n->_vd = alloc(vt->size);

	n->stats = NULL;
	n->name = NULL;
	n->_name = NULL;
	n->_name_long = NULL;

	n->id = max_id++;

	/* Default values */
	n->vectorize = 1;
	n->no_builtin = 0;
	n->samplelen = DEFAULT_SAMPLELEN;

	list_push(&vt->instances, n);

#ifdef WITH_HOOKS
	/* Add internal hooks if they are not already in the list */
	list_init(&n->hooks);
	if (!n->no_builtin) {
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

			list_push(&n->hooks, h);
		}
	}
#endif /* WITH_HOOKS */

	n->state = STATE_INITIALIZED;

	return 0;
}

int node_init2(struct node *n)
{
#ifdef WITH_HOOKS
	/* We sort the hooks according to their priority before starting the path */
	list_sort(&n->hooks, hook_cmp_priority);
#endif /* WITH_HOOKS */

	return 0;
}

int node_parse(struct node *n, json_t *cfg, const char *name)
{
	struct plugin *p;
	int ret;

	json_error_t err;
	json_t *json_hooks = NULL;

	const char *type;

	n->name = strdup(name);

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s?: i, s?: i, s?: o, s?: b }",
		"type", &type,
		"vectorize", &n->vectorize,
		"samplelen", &n->samplelen,
		"hooks", &json_hooks,
		"no_builtin", &n->no_builtin
	);
	if (ret)
		jerror(&err, "Failed to parse node '%s'", node_name(n));

	p = plugin_lookup(PLUGIN_TYPE_NODE, type);
	assert(&p->node == n->_vt);

#ifdef WITH_HOOKS
	if (json_hooks) {
		ret = hook_parse_list(&n->hooks, json_hooks, NULL, n);
		if (ret < 0)
			return ret;
	}
#endif /* WITH_HOOKS */

	ret = n->_vt->parse ? n->_vt->parse(n, cfg) : 0;
	if (ret)
		error("Failed to parse node '%s'", node_name(n));

	n->cfg = cfg;
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
	assert(n->state != STATE_DESTROYED);

	if (n->vectorize <= 0)
		error("Invalid `vectorize` value %d for node %s. Must be natural number!", n->vectorize, node_name(n));

	if (n->_vt->vectorize && n->_vt->vectorize < n->vectorize)
		error("Invalid value for `vectorize`. Node type requires a number smaller than %d!",
			n->_vt->vectorize);

	n->state = STATE_CHECKED;

	return 0;
}

int node_start(struct node *n)
{
	int ret;

	assert(n->state == STATE_CHECKED);

	info("Starting node %s", node_name_long(n));
	{ INDENT
#ifdef WITH_HOOKS
		for (size_t i = 0; i < list_length(&n->hooks); i++) {
			struct hook *h = (struct hook *) list_at(&n->hooks, i);

			ret = hook_start(h);
			if (ret)
				return ret;
		}
#endif /* WITH_HOOKS */

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

	if (n->state != STATE_STARTED)
		return 0;

	info("Stopping node %s", node_name(n));
	{ INDENT
#ifdef WITH_HOOKS
		for (size_t i = 0; i < list_length(&n->hooks); i++) {
			struct hook *h = (struct hook *) list_at(&n->hooks, i);

			ret = hook_stop(h);
			if (ret)
				return ret;
		}
#endif /* WITH_HOOKS */

		ret = n->_vt->stop ? n->_vt->stop(n) : 0;
	}

	if (ret == 0)
		n->state = STATE_STOPPED;

	return ret;
}

int node_destroy(struct node *n)
{
	assert(n->state != STATE_DESTROYED && n->state != STATE_STARTED);

#ifdef WITH_HOOKS
	list_destroy(&n->hooks, (dtor_cb_t) hook_destroy, true);
#endif

	if (n->_vt->destroy)
		n->_vt->destroy(n);

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

int node_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int readd, nread = 0;

	if (!n->_vt->read)
		return -1;

	/* Send in parts if vector not supported */
	if (n->_vt->vectorize > 0 && n->_vt->vectorize < cnt) {
		while (cnt - nread > 0) {
			readd = n->_vt->read(n, &smps[nread], MIN(cnt - nread, n->_vt->vectorize));
			if (readd < 0)
				return readd;

			nread += readd;
			debug(LOG_NODES | 5, "Received %u samples from node %s", readd, node_name(n));
		}
	}
	else {
		nread = n->_vt->read(n, smps, cnt);
		if (nread < 0)
			return nread;

		debug(LOG_NODES | 5, "Received %u samples from node %s", nread, node_name(n));
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
	int rread = hook_read_list(&n->hooks, smps, nread);
	if (nread != rread) {
		int skipped = nread - rread;

		debug(LOG_NODES | 10, "Hooks skipped %u out of %u samples for node %s", skipped, nread, node_name(n));

		if (n->stats)
			stats_update(n->stats, STATS_SKIPPED, skipped);
	}

	return rread;
#else
	return nread;
#endif /* WITH_HOOKS */
}

int node_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int sent, nsent = 0;

	if (!n->_vt->write)
		return -1;

#ifdef WITH_HOOKS
	/* Run write hooks */
	cnt = hook_write_list(&n->hooks, smps, cnt);
	if (cnt <= 0)
		return cnt;
#endif /* WITH_HOOKS */

	/* Send in parts if vector not supported */
	if (n->_vt->vectorize > 0 && n->_vt->vectorize < cnt) {
		while (cnt - nsent > 0) {
			sent = n->_vt->write(n, &smps[nsent], MIN(cnt - nsent, n->_vt->vectorize));
			if (sent < 0)
				return sent;

			nsent += sent;
			debug(LOG_NODES | 5, "Sent %u samples to node %s", sent, node_name(n));
		}
	}
	else {
		nsent = n->_vt->write(n, smps, cnt);
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
			strcatf(&n->_name_long, "%s: #hooks=%zu, id=%d, vectorize=%d, samplelen=%d, %s", node_name(n), list_length(&n->hooks), n->id, n->vectorize, n->samplelen, name_long);
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
