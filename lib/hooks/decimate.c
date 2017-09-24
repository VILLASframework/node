/** Decimate hook.
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

struct decimate {
	int ratio;
	unsigned counter;
};

static int decimate_init(struct hook *h)
{
	struct decimate *p = h->_vd;

	p->counter = 0;

	return 0;
}

static int decimate_parse(struct hook *h, json_t *cfg)
{
	struct decimate *p = h->_vd;

	int ret;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: i }",
		"ratio", &p->ratio
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", plugin_name(h->_vt));

	return 0;
}

static int decimate_read(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct decimate *p = h->_vd;

	int i, ok;
	for (i = 0, ok = 0; i < *cnt; i++) {
		if (p->counter++ % p->ratio == 0) {
			struct sample *tmp;

			tmp = smps[ok];
			smps[ok++] = smps[i];
			smps[i] = tmp;
		}
	}

	*cnt = ok;

	return 0;
}

static struct plugin p = {
	.name		= "decimate",
	.description	= "Downsamping by integer factor",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags	= HOOK_NODE | HOOK_PATH,
		.priority = 99,
		.init	= decimate_init,
		.parse	= decimate_parse,
		.read	= decimate_read,
		.size	= sizeof(struct decimate)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
