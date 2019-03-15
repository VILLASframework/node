/** Energy-based Metric hook.
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

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/sample.h>

struct ebm {

};

static int ebm_init(struct hook *h)
{
	__attribute__((unused)) struct ebm *e = (struct ebm *) h->_vd;

	return 0;
}

static int ebm_destroy(struct hook *h)
{
	__attribute__((unused)) struct ebm *e = (struct ebm *) h->_vd;

	return 0;
}

static int ebm_start(struct hook *h)
{
	__attribute__((unused)) struct ebm *e = (struct ebm *) h->_vd;

	return 0;
}

static int ebm_stop(struct hook *h)
{
	__attribute__((unused)) struct ebm *e = (struct ebm *) h->_vd;

	return 0;
}

static int ebm_parse(struct hook *h, json_t *cfg)
{
	__attribute__((unused)) struct ebm *e = (struct ebm *) h->_vd;

	return 0;
}

static int ebm_prepare(struct hook *h)
{
	__attribute__((unused)) struct ebm *e = (struct ebm *) h->_vd;

	return 0;
}


static int ebm_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	__attribute__((unused)) struct ebm *e = (struct ebm *) h->_vd;

	return 0;
}

static struct plugin p = {
	.name		= "ebm",
	.description	= "Energy-based Metric",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_PATH | HOOK_NODE_READ | HOOK_NODE_WRITE,
		.priority	= 99,
		.init		= ebm_init,
		.init_signals	= ebm_prepare,
		.destroy	= ebm_destroy,
		.start		= ebm_start,
		.stop		= ebm_stop,
		.parse		= ebm_parse,
		.process	= ebm_process,
		.size		= sizeof(struct ebm)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
