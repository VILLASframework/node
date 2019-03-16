
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
#include <villas/sample.h>

struct cast {
	int signal_index;
	char *signal_name;

	enum signal_type new_type;
	char *new_name;
	char *new_unit;
};

static int cast_prepare(struct hook *h)
{
	struct signal *orig_sig, *new_sig;
	struct cast *c = (struct cast *) h->_vd;

	if (c->signal_name) {
		c->signal_index = vlist_lookup_index(&h->signals, c->signal_name);
		if (c->signal_index < 0)
			return -1;
	}

	char *name, *unit;
	enum signal_type type;

	orig_sig = vlist_at_safe(&h->signals, c->signal_index);

	type = c->new_type != SIGNAL_TYPE_INVALID ? c->new_type : orig_sig->type;
	name = c->new_name ? c->new_name : orig_sig->name;
	unit = c->new_unit ? c->new_unit : orig_sig->unit;

	new_sig = signal_create(name, unit, type);

	vlist_set(&h->signals, c->signal_index, new_sig);
	signal_decref(orig_sig);

	return 0;
}

static int cast_destroy(struct hook *h)
{
	struct cast *c = (struct cast *) h->_vd;

	if (c->signal_name)
		free(c->signal_name);

	if (c->new_name)
		free(c->new_name);

	if (c->new_unit)
		free(c->new_unit);

	return 0;
}

static int cast_parse(struct hook *h, json_t *cfg)
{
	int ret;
	struct cast *c = (struct cast *) h->_vd;

	json_error_t err;
	json_t *json_signal;

	const char *new_name = NULL;
	const char *new_unit = NULL;
	const char *new_type = NULL;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s?: s, s?: s, s?: s }",
		"signal", &json_signal,
		"new_type", &new_type,
		"new_name", &new_name,
		"new_unit", &new_unit
	);
	if (ret)
		return ret;

	switch (json_typeof(json_signal)) {
		case JSON_STRING:
			c->signal_name = strdup(json_string_value(json_signal));
			break;

		case JSON_INTEGER:
			c->signal_name = NULL;
			c->signal_index = json_integer_value(json_signal);
			break;

		default:
			error("Invalid value for setting 'signal' in hook '%s'", hook_type_name(h->_vt));
	}

	if (new_type) {
		c->new_type = signal_type_from_str(new_type);
		if (c->new_type == SIGNAL_TYPE_INVALID)
			return -1;
	}
	else
		c->new_type = SIGNAL_TYPE_INVALID; // We use this constant to indicate that we dont want to change the type

	if (new_name)
		c->new_name = strdup(new_name);

	if (new_unit)
		c->new_unit = strdup(new_unit);

	return 0;
}

static int cast_process(struct hook *h, struct sample *smp)
{
	struct cast *c = (struct cast *) h->_vd;

	struct signal *orig_sig = vlist_at(smp->signals, c->signal_index);
	struct signal *new_sig  = vlist_at(&h->signals,  c->signal_index);

	signal_data_cast(&smp->data[c->signal_index], orig_sig, new_sig);

	/* Replace signal descriptors of sample */
	smp->signals = &h->signals;

	return HOOK_OK;
}

static struct plugin p = {
	.name		= "cast",
	.description	= "Cast signals types",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_NODE_READ | HOOK_PATH,
		.priority	= 99,
		.destroy	= cast_destroy,
		.init_signals	= cast_prepare,
		.parse		= cast_parse,
		.process	= cast_process,
		.size		= sizeof(struct cast)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
