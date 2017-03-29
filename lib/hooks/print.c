/** Print hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include "hook.h"
#include "plugin.h"
#include "sample.h"

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
		sample_fprint(p->output, smps[i], SAMPLE_ALL);

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