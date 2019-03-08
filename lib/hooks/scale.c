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

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/sample.h>

struct scale {
	char *signal_name;
	int signal_index;

	double scale;
	double offset;
};

static int scale_init(struct hook *h)
{
	struct scale *s = (struct scale *) h->_vd;

	s->scale = 1;
	s->offset = 0;

	return 0;
}

static int scale_prepare(struct hook *h)
{
	struct scale *s = (struct scale *) h->_vd;

	if (s->signal_name) {
		s->signal_index = vlist_lookup_index(&h->signals, s->signal_name);
		if (s->signal_index < 0)
			return -1;
	}

	return 0;
}

static int scale_destroy(struct hook *h)
{
	struct scale *s = (struct scale *) h->_vd;

	if (s->signal_name)
		free(s->signal_name);

	return 0;
}

static int scale_parse(struct hook *h, json_t *cfg)
{
	struct scale *s = (struct scale *) h->_vd;

	int ret;
	json_t *json_signal;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: F, s?: F, s: o }",
		"scale", &s->scale,
		"offset", &s->offset,
		"signal", &json_signal
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", hook_type_name(h->_vt));

	switch (json_typeof(json_signal)) {
		case JSON_STRING:
			s->signal_name = strdup(json_string_value(json_signal));
			break;

		case JSON_INTEGER:
			s->signal_name = NULL;
			s->signal_index = json_integer_value(json_signal);
			break;

		default:
			error("Invalid value for setting 'signal' in hook '%s'", hook_type_name(h->_vt));
	}

	return 0;
}

static int scale_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct scale *s = (struct scale *) h->_vd;

	for (int i = 0; i < *cnt; i++) {
		struct sample *smp = smps[i];
		int k = s->signal_index;

		switch (sample_format(smp, k)) {
			case SIGNAL_TYPE_INTEGER:
				smp->data[k].i = smp->data[k].i * s->scale + s->offset;
				break;

			case SIGNAL_TYPE_FLOAT:
				smp->data[k].f = smp->data[k].f * s->scale + s->offset;
				break;

			case SIGNAL_TYPE_COMPLEX:
				smp->data[k].z = smp->data[k].z * s->scale + s->offset;
				break;

			case SIGNAL_TYPE_BOOLEAN:
				smp->data[k].b = smp->data[k].b * s->scale + s->offset;
				break;

			default: { }
		}
	}

	return 0;
}

static struct plugin p = {
	.name		= "scale",
	.description	= "Scale all signals by and add offset",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_PATH,
		.priority	= 99,
		.init		= scale_init,
		.init_signals	= scale_prepare,
		.destroy	= scale_destroy,
		.parse		= scale_parse,
		.process	= scale_process,
		.size		= sizeof(struct scale)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
