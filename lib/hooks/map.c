/** Override a value of the sample with a header, timestamp or statistic value.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include "plugin.h"
#include "mapping.h"
#include "list.h"
#include "utils.h"
#include "path.h"

struct map {
	struct mapping mapping;

	struct stats *stats;
};

static int map_init(struct hook *h)
{
	struct map *p = h->_vd;

	return mapping_init(&p->mapping);
}

static int map_destroy(struct hook *h)
{
	struct map *p = h->_vd;

	return mapping_destroy(&p->mapping);
}

static int map_parse(struct hook *h, config_setting_t *cfg)
{
	int ret;
	struct map *p = h->_vd;
	config_setting_t *cfg_mapping;
	
	cfg_mapping = config_setting_lookup(cfg, "mapping");
	if (!cfg_mapping || !config_setting_is_array(cfg_mapping))
		return -1;
	
	ret = mapping_parse(&p->mapping, cfg_mapping);
	if (ret)
		return ret;

	return 0;
}

static int map_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	int ret;
	struct map *p = h->_vd;
	struct sample *tmp[*cnt];

	if (*cnt <= 0)
		return 0;

	ret = sample_alloc(smps[0]->pool, tmp, *cnt);
	if (ret != *cnt)
		return ret;

	for (int i = 0; i < *cnt; i++) {
		mapping_remap(&p->mapping, smps[i], tmp[i], NULL);
	
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
		.priority = 99,
		.init	= map_init,
		.destroy= map_destroy,
		.parse	= map_parse,
		.read	= map_read,
		.size	= sizeof(struct map)
	}
};

REGISTER_PLUGIN(&p);

/** @} */