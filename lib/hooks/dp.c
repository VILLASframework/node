/** Dynamic Phasor Interface Algorithm hook.
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

#include <string.h>

#include "villas/hook.h"
#include "villas/plugin.h"
#include "villas/sample.h"
#include "villas/bitset.h"

struct dp_series {
	int idx;
	int *harmonics;
	int nharmonics;
	double f0;
	double rate;
	enum {
		DP_INTERPOLATION_NONE,
		DP_INTERPOLATION_STEP,
		DP_INTERPOLATION_LINEAR
	} interpolation;

	double *history;

	unsigned counter;
	unsigned last_sequence;
};

struct dp {
	struct list series;
};

static double dp_series_step(struct dp_series *s, double val)
{

}

static double dp_series_istep(struct dp_series *s, double val)
{

}

static int dp_series_init(struct dp_series *s, int idx, double f0, double rate)
{
	d->idx = idx;
	d->f0 = f0;
}

static int dp_series_destroy(struct dp_series *s)
{
	free(s->history);

	return 0;
}

static int dp_init(struct hook *h)
{
	struct dp *p = (struct dp *) h->_vd;

	list_init(&p->series);

	return 0;
}

static int dp_deinit(struct hook *h)
{
	struct dp *p = (struct dp *) h->_vd;

	bitset_destroy(p->mask, (dtor_cb_t) dp_series_destroy, true);

	return 0;
}

static int dp_parse(struct hook *h, json_t *cfg)
{
	struct dp *p = (struct dp *) h->_vd;

	int ret;
	json_error_t err;
	const char *mode = NULL;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: F, s?: i }",
		"f0", &p->f0,
		"mask", &p->mask
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", plugin_name(h->_vt));

	return 0;
}

static int dp_read(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct dp *p = (struct dp *) h->_vd;

	for (unsigned j = 0; j < cnt; j++) {
		struct sample *t = smps[j];

		for (size_t i = 0; i < list_length(&p->series); i++) {
			struct dp_series *s = (struct dp_series *) list_at(&p->series, i);

			if (s->idx > t->length)
				continue;
		}
	}

	return 0;
}

static int dp_write(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct dp *p = (struct dp *) h->_vd;

	return 0;
}

static struct plugin p = {
	.name		= "dp",
	.description	= "Transform to dynamic phasor domain",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags	= HOOK_NODE,
		.priority = 99,
		.init	= dp_init,
		.parse	= dp_parse,
		.read	= dp_read,
		.write	= dp_write,
		.size	= sizeof(struct dp)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
