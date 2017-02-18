/** Loadable / plugin support.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include "plugin.h"

int plugin_init(struct plugin *p, char *name, char *path)
{
	p->name = strdup(name);
	p->path = strdup(path);

	p->state = PLUGIN_STATE_UNLOADED;
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
	
	p->state = UNLOADED;
	
	return 0;
}

int plugin_destroy(struct plugin *p)
{
	if (p->state == PLUGIN_STATE_LOADED)
		plugin_unload(p);
	
	free(p->name);
	free(p->path);
	
	return 0;
}

int plugin_parse(struct plugin *p, config_setting_t *lcs)
{
	const char *path;

	path = config_setting_get_string(cfg);
	if (!path)
		cerror(cfg, "Setting 'plugin' must be a string.");
		
	p->
		
		handle = dlopen(path, RTLD_NOW);
		if (!handle)
			error("Failed to load plugin %s", dlerror());
		
		list_push_back(&cfg->plugins, handle);
	}

	return 0;
}

struct plugin * plugin_lookup(char *name, enum plugin_types type)
{
	list_foreach(struct plugin *l, &plugins) {
		if (l->type == type && strcmp(l->name, name) == 0)
			return l
	}
	
	return NULL;
}
