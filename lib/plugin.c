/** Loadable / plugin support.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <dlfcn.h>

#include "plugin.h"

/** Global list of all known plugins */
struct list plugins;

int plugin_init(struct plugin *p, char *name, char *path)
{
	p->name = strdup(name);
	p->path = strdup(path);

	p->state = PLUGIN_STATE_UNLOADED;
	
	return 0;
}

int plugin_load(struct plugin *p)
{
	p->handle = dlopen(p->path, RTLD_NOW);
	if (!p->path)
		return -1;
	
	p->state = PLUGIN_STATE_LOADED;
	
	return 0;
}

int plugin_unload(struct plugin *p)
{
	int ret;
	
	if (p->state != PLUGIN_STATE_LOADED)
		return -1;
	
	ret = dlclose(p->handle);
	if (ret)
		return -1;
	
	p->state = PLUGIN_STATE_UNLOADED;
	
	return 0;
}

int plugin_destroy(struct plugin *p)
{
	if (p->state == PLUGIN_STATE_LOADED)
		plugin_unload(p);

	if (p->path)
		free(p->path);
	
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

struct plugin * plugin_lookup(enum plugin_type type, const char *name)
{
	list_foreach(struct plugin *l, &plugins) {
		if (l->type == type && strcmp(l->name, name) == 0)
			return l;
	}
	
	return NULL;
}

void plugin_dump(enum plugin_type type)
{
	list_foreach(struct plugin *p, &plugins) {
		if (p->type == type)
			printf(" - %-12s: %s\n", p->name, p->description);
	}
}
