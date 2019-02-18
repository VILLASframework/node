
/** Cast hook.
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

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/node.h>
#include <villas/path.h>
#include <villas/sample.h>

struct cast {
	struct vlist operations;

	struct vlist signals;
};

static int cast_init(struct hook *h)
{
	int ret;
	struct cast *c = (struct cast *) h->_vd;
	struct vlist *orig_signals;

	if (h->node)
		orig_signals = &h->node->in.signals;
	else if (h->path)
		orig_signals = &h->path->signals;
	else
		return -1;

	ret = vlist_init(&c->signals);
	if (ret)
		return ret;

	/* Copy original signal list */
	for (int i = 0; i < vlist_length(orig_signals); i++) {
		struct signal *orig_sig = vlist_at(orig_signals, i);
		struct signal *new_sig = signal_copy(orig_sig);

		vlist_push(&c->signals, new_sig);
	}

	return 0;
}

static int cast_destroy(struct hook *h)
{
	int ret;
	struct cast *c = (struct cast *) h->_vd;

	ret = vlist_destroy(&c->signals, (dtor_cb_t) signal_decref, false);
	if (ret)
		return ret;

	return 0;
}

static int cast_parse(struct hook *h, json_t *cfg)
{
	int ret;
	struct cast *c = (struct cast *) h->_vd;
	struct signal *sig;

	size_t i;
	json_t *json_signals;
	json_t *json_signal;

	ret = json_unpack(cfg, "{ s: o }",
		"signals", &json_signals
	);
	if (ret)
		return ret;

	if (json_is_array(json_signals))
		return -1;

	json_array_foreach(json_signals, i, json_signal) {
		int index = -1;
		const char *name = NULL;

		const char *new_name = NULL;
		const char *new_unit = NULL;
		const char *new_format = NULL;

		ret = json_unpack(json_signal, "{ s?: s, s?: i, s?: s, s?: s, s?: s }",
			"name", &name,
			"index", &index,
			"new_format", &new_format,
			"new_name", &new_name,
			"new_unit", &new_unit
		);
		if (ret)
			return ret;

		/* Find matching original signal descriptor */
		if (index >= 0 && name != NULL)
			return -1;

		if (index < 0 && name == NULL)
			return -1;

		sig = name
			? vlist_lookup(&c->signals, name)
			: vlist_at_safe(&c->signals, index);
		if (!sig)
			return -1;

		/* Cast to new format */
		if (new_format) {
			enum signal_type fmt;

			fmt = signal_type_from_str(new_format);
			if (fmt == SIGNAL_TYPE_INVALID)
				return -1;

			sig->type = fmt;
		}

		/* Set new name */
		if (new_name) {
			if (sig->name)
				free(sig->name);

			sig->name = strdup(new_name);
		}

		/* Set new unit */
		if (new_unit) {
			if (sig->unit)
				free(sig->unit);

			sig->unit = strdup(new_unit);
		}
	}

	return 0;
}

static int cast_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct cast *c = (struct cast *) h->_vd;

	for (int i = 0; i < *cnt; i++) {
		struct sample *smp = smps[i];

		for (int j = 0; j < smp->length; j++) {
			struct signal *orig_sig = vlist_at(smp->signals, j);
			struct signal *new_sig = vlist_at(&c->signals, j);

			signal_data_cast(&smp->data[j], orig_sig, new_sig);
		}

		/* Replace signal descriptors of sample */
		smp->signals = &c->signals;
	}

	return 0;
}

static struct plugin p = {
	.name		= "cast",
	.description	= "Cast signals",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_NODE_READ | HOOK_PATH,
		.priority	= 99,
		.init		= cast_init,
		.destroy	= cast_destroy,
		.parse		= cast_parse,
		.process	= cast_process,
		.size		= sizeof(struct cast)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
