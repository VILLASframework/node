/** A simple example hook function which can be loaded as a plugin.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stddef.h>

#include <villas/hook.h>
#include <villas/log.h>
#include <villas/plugin.h>

struct hook;
struct path;
struct sample;

static int hook_example(struct hook *h, int when, struct hook_info *j)
{
	info("Hello world from example hook!");
	
	return 0;
}

static struct plugin p = {
	.name		= "example",
	.description	= "This is just a simple example hook",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.cb	= hook_example,
		.when	= HOOK_PATH_START
	}
};

REGISTER_PLUGIN(&p)