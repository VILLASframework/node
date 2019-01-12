/** Loadable / plugin support.
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

#include <dlfcn.h>
#include <string.h>

#include <villas/plugin.h>

/** Global list of all known plugins */
struct vlist plugins = { .state = STATE_DESTROYED };

LIST_INIT_STATIC(&plugins)

int plugin_init(struct plugin *p)
{
	assert(p->state == STATE_DESTROYED);

	p->state = STATE_INITIALIZED;

	return 0;
}

int plugin_parse(struct plugin *p, json_t *cfg)
{
	const char *path;

	path = json_string_value(cfg);
	if (!path)
		return -1;

	p->path = strdup(path);

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
	for (size_t i = 0; i < vlist_length(&plugins); i++) {
		struct plugin *p = (struct plugin *) vlist_at(&plugins, i);

		if (p->type == type && strcmp(p->name, name) == 0)
			return p;
	}

	return NULL;
}

void plugin_dump(enum plugin_type type)
{
	for (size_t i = 0; i < vlist_length(&plugins); i++) {
		struct plugin *p = (struct plugin *) vlist_at(&plugins, i);

		if (p->type == type)
			printf(" - %-13s: %s\n", p->name, p->description);
	}
}
