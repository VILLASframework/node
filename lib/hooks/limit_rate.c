/** Rate-limiting hook.
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

#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/timing.h>
#include <villas/sample.h>

struct limit_rate {
	enum {
		LIMIT_RATE_LOCAL,
		LIMIT_RATE_RECEIVED,
		LIMIT_RATE_ORIGIN
	} mode; /**< The timestamp which should be used for limiting. */

	double deadtime;
	struct timespec last;
};

static int limit_rate_init(struct hook *h)
{
	struct limit_rate *p = (struct limit_rate *) h->_vd;

	p->last = (struct timespec) { 0 };

	/* Default values */
	p->mode = LIMIT_RATE_LOCAL;

	return 0;
}

static int limit_rate_parse(struct hook *h, json_t *cfg)
{
	struct limit_rate *p = (struct limit_rate *) h->_vd;

	int ret;
	json_error_t err;

	double rate;
	const char *mode = NULL;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: F, s?: s }",
		"rate", &rate,
		"mode", &mode
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", hook_type_name(h->_vt));

	if (mode) {
		if (!strcmp(mode, "origin"))
			p->mode = LIMIT_RATE_ORIGIN;
		else if (!strcmp(mode, "received"))
			p->mode = LIMIT_RATE_RECEIVED;
		else if (!strcmp(mode, "local"))
			p->mode = LIMIT_RATE_LOCAL;
		else
			error("Invalid value '%s' for setting 'mode' in limit_rate hook", mode);
	}

	p->deadtime = 1.0 / rate;

	return 0;
}

static int limit_rate_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	struct limit_rate *p = (struct limit_rate *) h->_vd;

	struct timespec next;
	unsigned ret = 0;

	for (unsigned i = 0; i < *cnt; i++) {
		switch (p->mode) {
			case LIMIT_RATE_LOCAL:
				next = time_now();
				break;

			case LIMIT_RATE_ORIGIN:
				next = smps[i]->ts.origin;
				break;

			case LIMIT_RATE_RECEIVED:
				next = smps[i]->ts.received;
				break;
		}

		if (time_delta(&p->last, &next) < p->deadtime)
			continue; /* Drop this sample */

		p->last = next;
		smps[ret++] = smps[i];
	}

	*cnt = ret;

	return 0;
}

static struct plugin p = {
	.name		= "limit_rate",
	.description	= "Limit sending rate",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.flags		= HOOK_NODE_READ | HOOK_NODE_WRITE | HOOK_PATH,
		.priority	= 99,
		.init		= limit_rate_init,
		.parse		= limit_rate_parse,
		.process	= limit_rate_process,
		.size		= sizeof(struct limit_rate)
	}
};

REGISTER_PLUGIN(&p)

/** @} */
