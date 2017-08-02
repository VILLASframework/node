/** Convert hook.
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

/** @addtogroup hooks Hook functions
 * @{
 */

#include "hook.h"
#include "plugin.h"
#include "sample.h"

struct convert {
	enum {
		TO_FIXED,
		TO_FLOAT
	} mode;

	double scale;

	long long mask;
};

static int convert_init(struct hook *h)
{
	struct convert *p = h->_vd;

	p->scale = 1;
	p->mask = -1;

	return 0;
}

static int convert_parse(struct hook *h, json_t *cfg)
{
	struct convert *p = h->_vd;

	int ret;
	json_error_t err;
	const char *mode = NULL;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: F, s?: i, s: s }",
		"scale", &p->scale,
		"mask", &p->mask,
		"mode", &mode
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", plugin_name(h->_vt));

	if      (!strcmp(mode, "fixed"))
		p->mode = TO_FIXED;
	else if (!strcmp(mode, "float"))
		p->mode = TO_FLOAT;
	else
		error("Invalid 'mode' setting '%s' for hook '%s'", mode, plugin_name(h->_vt));

	return 0;
}

static int convert_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	struct convert *p = h->_vd;

	for (int i = 0; i < *cnt; i++) {
		for (int k = 0; k < smps[i]->length; k++) {

			/* Only convert values which are not masked */
			if ((k < sizeof(p->mask) * 8) && !(p->mask & (1LL << k)))
				continue;

			switch (p->mode) {
				case TO_FIXED:
					smps[i]->data[k].i = smps[i]->data[k].f * p->scale;
					sample_set_data_format(smps[i], k, SAMPLE_DATA_FORMAT_INT);
					break;
				case TO_FLOAT:
					smps[i]->data[k].f = smps[i]->data[k].i * p->scale;
					sample_set_data_format(smps[i], k, SAMPLE_DATA_FORMAT_FLOAT);
					break;
			}
		}
	}

	return 0;
}

static struct plugin p = {
	.name		= "convert",
	.description	= "Convert message from / to floating-point / integer",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.init	= convert_init,
		.parse	= convert_parse,
		.read	= convert_read,
		.size	= sizeof(struct convert)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
