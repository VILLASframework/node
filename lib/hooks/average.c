/** Average hook.
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

struct average {
	int mask;
	int offset;
};

static int average_parse(struct hook *h, json_t *cfg)
{
	struct average *p = (struct average *) h->_vd;

	int ret;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: i, s: i }",
		"offset", &p->offset,
		"mask", &p->mask
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", hook_type_name(h->_vt));

	return 0;
}

static int average_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct average *p = (struct average *) h->_vd;

	for (int i = 0; i < *cnt; i++) {
		struct sample *smp = smps[i];
		double sum = 0;
		int n = 0;

		for (int k = 0; k < smp->length; k++) {
			if (!(p->mask & (1 << k)))
				continue;

			switch (sample_format(smps[i], k)) {
				case SIGNAL_TYPE_INTEGER:
					sum += smp->data[k].i;
					break;

				case SIGNAL_TYPE_FLOAT:
					sum += smp->data[k].f;
					break;

				case SIGNAL_TYPE_AUTO:
				case SIGNAL_TYPE_INVALID:
				case SIGNAL_TYPE_COMPLEX:
				case SIGNAL_TYPE_BOOLEAN:
					return -1; /* not supported */
			}

			n++;
		}

		smp->data[p->offset].f = sum / n;
	}

	return 0;
}

static struct plugin p = {
	.name		= "average",
	.description	= "Calculate average over some signals",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_PATH | HOOK_NODE_READ | HOOK_NODE_WRITE,
		.priority	= 99,
		.parse		= average_parse,
		.process	= average_process,
		.size		= sizeof(struct average)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
