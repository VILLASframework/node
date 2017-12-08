/** Shift sequence number of samples
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

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/sample.h>

struct shift {
	int offset;
};

static int shift_seq_parse(struct hook *h, json_t *cfg)
{
	struct shift *p = (struct shift *) h->_vd;

	json_error_t err;
	int ret;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: i }",
		"offset", &p->offset
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", plugin_name(h->_vt));

	return 0;
}

static int shift_seq_read(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct shift *p = (struct shift *) h->_vd;

	for (int i = 0; i < *cnt; i++)
		smps[i]->sequence += p->offset;

	return 0;
}

static struct plugin p = {
	.name		= "shift_seq",
	.description	= "Shift sequence number of samples",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags	= HOOK_NODE | HOOK_PATH,
		.priority = 99,
		.parse	= shift_seq_parse,
		.read	= shift_seq_read,
		.size	= sizeof(struct shift),
	}
};

REGISTER_PLUGIN(&p)

/** @} */
