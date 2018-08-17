/** Scale hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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
	double scale;
	double offset;
};

static int scale_init(struct hook *h)
{
	struct scale *p = (struct scale *) h->_vd;

	p->scale = 1;
	p->offset = 0;

	return 0;
}

static int scale_parse(struct hook *h, json_t *cfg)
{
	struct scale *p = (struct scale *) h->_vd;

	int ret;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: F, s?: F }",
		"scale", &p->scale,
		"offset", &p->offset
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", plugin_name(h->_vt));

	return 0;
}

static int scale_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct scale *p = (struct scale *) h->_vd;

	for (int i = 0; i < *cnt; i++) {
		for (int k = 0; k < smps[i]->length; k++) {

			switch (sample_format(smps[i], k)) {
				case SIGNAL_FORMAT_INTEGER:
					smps[i]->data[k].i = smps[i]->data[k].i * p->scale + p->offset;
					break;

				case SIGNAL_FORMAT_FLOAT:
					smps[i]->data[k].f = smps[i]->data[k].f * p->scale + p->offset;
					break;

				case SIGNAL_FORMAT_COMPLEX:
					smps[i]->data[k].z = smps[i]->data[k].z * p->scale + p->offset;
					break;

				case SIGNAL_FORMAT_BOOLEAN:
					smps[i]->data[k].b = smps[i]->data[k].b * p->scale + p->offset;
					break;

				default: { }
			}
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
		.parse		= scale_parse,
		.process	= scale_process,
		.size		= sizeof(struct scale)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
