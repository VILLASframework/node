/** Print hook.
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
#include "sample_io.h"

struct print {
	FILE *output;
	const char *uri;
};

static int print_init(struct hook *h)
{
	struct print *p = h->_vd;

	p->output = stdout;
	p->uri = NULL;

	return 0;
}

static int print_start(struct hook *h)
{
	struct print *p = h->_vd;

	if (p->uri) {
		p->output = fopen(p->uri, "w+");
		if (!p->output)
			error("Failed to open file %s for writing", p->uri);
	}

	return 0;
}

static int print_stop(struct hook *h)
{
	struct print *p = h->_vd;

	if (p->uri)
		fclose(p->output);

	return 0;
}

static int print_parse(struct hook *h, config_setting_t *cfg)
{
	struct print *p = h->_vd;

	config_setting_lookup_string(cfg, "output", &p->uri);

	return 0;
}

static int print_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	struct print *p = h->_vd;

	for (int i = 0; i < *cnt; i++)
		sample_io_villas_fprint(p->output, smps[i], SAMPLE_IO_ALL);

	return 0;
}

static struct plugin p = {
	.name		= "print",
	.description	= "Print the message to stdout",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.init	= print_init,
		.parse	= print_parse,
		.start	= print_start,
		.stop	= print_stop,
		.read	= print_read,
		.size	= sizeof(struct print)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
