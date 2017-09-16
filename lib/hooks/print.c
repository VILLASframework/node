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
#include "io.h"

struct print {
	struct io io;
	struct io_format *format;

	const char *uri;
};

static int print_init(struct hook *h)
{
	struct print *p = h->_vd;

	p->uri = NULL;
	p->format = io_format_lookup("villas-human");

	return 0;
}

static int print_start(struct hook *h)
{
	struct print *p = h->_vd;
	int ret;

	ret = io_init(&p->io, p->format, SAMPLE_HAS_ALL);
	if (ret)
		return ret;

	ret = io_open(&p->io, p->uri);
	if (ret)
		return ret;

	return 0;
}

static int print_stop(struct hook *h)
{
	struct print *p = h->_vd;
	int ret;

	ret = io_close(&p->io);
	if (ret)
		return ret;

	ret = io_destroy(&p->io);
	if (ret)
		return ret;

	return 0;
}

static int print_parse(struct hook *h, json_t *cfg)
{
	struct print *p = h->_vd;
	const char *format = NULL;
	int ret;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s }",
		"output", &p->uri,
		"format", &format
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", plugin_name(h->_vt));

	if (format) {
		p->format = io_format_lookup(format);
		if (!p->format)
			jerror(&err, "Invalid format '%s'", format);
	}

	return 0;
}

static int print_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct print *p = h->_vd;

	io_print(&p->io, smps, *cnt);

	return 0;
}

static struct plugin p = {
	.name		= "print",
	.description	= "Print the message to stdout",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags	= HOOK_NODE | HOOK_PATH,
		.priority = 99,
		.init	= print_init,
		.parse	= print_parse,
		.start	= print_start,
		.stop	= print_stop,
		.process= print_process,
		.size	= sizeof(struct print)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
