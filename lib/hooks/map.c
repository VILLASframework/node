/** Override a value of the sample with a header, timestamp or statistic value.
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

#include "plugin.h"
#include "mapping.h"
#include "list.h"
#include "utils.h"
#include "path.h"
#include "sample.h"
#include "node.h"

struct map {
	struct list mapping;

	struct stats *stats;
};

static int map_init(struct hook *h)
{
	struct map *m = h->_vd;

	m->stats = NULL; /** @todo */

	return list_init(&m->mapping);
}

static int map_destroy(struct hook *h)
{
	struct map *m = h->_vd;

	return list_destroy(&m->mapping, NULL, true);
}

static int map_parse(struct hook *h, json_t *cfg)
{
	int ret;
	struct map *m = h->_vd;
	json_error_t err;
	json_t *cfg_mapping;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: o }",
		"map", &cfg_mapping
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", plugin_name(h->_vt));

	ret = mapping_parse_list(&m->mapping, cfg_mapping, NULL);
	if (ret)
		return ret;

	return 0;
}

static int map_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	int ret;
	struct map *m = h->_vd;
	struct sample *tmp[*cnt];
	struct pool *p;

	if (*cnt <= 0)
		return 0;

	p = sample_pool(smps[0]);
	if (!p)
		return -1;

	ret = sample_alloc(p, tmp, *cnt);
	if (ret != *cnt)
		return ret;

	for (int i = 0; i < *cnt; i++) {
		tmp[i]->format   = 0;
		tmp[i]->length   = 0;

		mapping_remap(&m->mapping, tmp[i], smps[i], m->stats);

		SWAP(smps[i], tmp[i]);
	}

	sample_free(tmp, *cnt);

	return 0;
}

static struct plugin p = {
	.name		= "map",
	.description	= "Remap values and / or add header, timestamp values to the sample",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags	= HOOK_PATH,
		.priority = 99,
		.init	= map_init,
		.destroy= map_destroy,
		.parse	= map_parse,
		.process= map_process,
		.size	= sizeof(struct map)
	}
};

REGISTER_PLUGIN(&p);

/** @} */
