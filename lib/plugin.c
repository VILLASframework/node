/** Loadable / plugin support.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <dlfcn.h>

#include "plugin.h"

/** Global list of all known plugins */
struct list plugins = LIST_INIT();

__attribute__((destructor(999))) static void __dtor_plugins()
{
	list_destroy(&plugins, NULL, false);
}

int plugin_init(struct plugin *p)
{
	assert(p->state == STATE_DESTROYED);

	p->state = STATE_INITIALIZED;
	
	return 0;
}

int plugin_parse(struct plugin *p, config_setting_t *cfg)
{
	const char *path;

	path = config_setting_get_string(cfg);
	if (!path)
		cerror(cfg, "Setting 'plugin' must be a string.");

	return 0;
}

int plugin_load(struct plugin *p)
{
	p->handle = dlopen(p->path, RTLD_NOW);
	if (!p->path)
		return -1;
	
	p->state = STATE_LOADED;
	
	return 0;
}

int plugin_unload(struct plugin *p)
{
	int ret;
	
	assert(p->state == STATE_LOADED);
	
	ret = dlclose(p->handle);
	if (ret)
		return -1;
	
	p->state = STATE_UNLOADED;
	
	return 0;
}

int plugin_destroy(struct plugin *p)
{
	assert(p->state != STATE_DESTROYED && p->state != STATE_LOADED);

	if (p->path)
		free(p->path);
	
	return 0;
}

struct plugin * plugin_lookup(enum plugin_type type, const char *name)
{
	for (size_t i = 0; i < list_length(&plugins); i++) {
		struct plugin *p = list_at(&plugins, i);

		if (p->type == type && strcmp(p->name, name) == 0)
			return p;
	}
	
	return NULL;
}

void plugin_dump(enum plugin_type type)
{
	for (size_t i = 0; i < list_length(&plugins); i++) {
		struct plugin *p = list_at(&plugins, i);

		if (p->type == type)
			printf(" - %-12s: %s\n", p->name, p->description);
	}
}
