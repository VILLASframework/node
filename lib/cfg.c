/** Configuration parser.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <dlfcn.h>
#include <unistd.h>

#include "utils.h"
#include "list.h"
#include "cfg.h"
#include "node.h"
#include "path.h"
#include "hook.h"

static int cfg_parse_log(config_setting_t *cfg, struct settings *set)
{
	config_setting_t *cfg_facilities;
	
	if (!config_setting_lookup_int(cfg, "level", &set->log.level))
		set->log.level = V;
	
	if (!config_setting_lookup_string(cfg, "file", &set->log.file))
		set->log.file = NULL;
	
	cfg_facilities = config_setting_get_member(cfg, "facilities");
	if (cfg_facilities) {
		set->log.facilities = 0;
		
		for (int i = 0; i < config_setting_length(cfg); i++) {
			int facility;
			const char *str;
			config_setting_t *elm = config_setting_get_elem(cfg, i);
			
			str = config_setting_get_string(elm);
			if (!str)
				cerror(elm, "Facilties must be configured as an array of strings");
			
			facility = log_lookup_facility(str);
			if (!facility)
				cerror(elm, "%s is an unknown debug facility", str);
			
			set->log.facilities |= facility;
		}
	}
	else /* By default we enable all faciltities */
		set->log.facilities = ~0;
	
	return 0;
}

static int cfg_parse_plugins(config_setting_t *cfg)
{
	if (!config_setting_is_array(cfg))
		cerror(cfg, "Setting 'plugins' must be a list of strings");

	for (int i = 0; i < config_setting_length(cfg); i++) {
		void *handle;
		const char *path;

		path = config_setting_get_string_elem(cfg, i);
		if (!path)
			cerror(cfg, "Setting 'plugins' must be a list of strings");
		
		handle = dlopen(path, RTLD_NOW);
		if (!handle)
			error("Failed to load plugin %s", dlerror());
	}

	return 0;
}

static int cfg_parse_global(config_setting_t *cfg, struct settings *set)
{
	config_setting_t *cfg_plugins, *cfg_log;

	if (!config_setting_lookup_int(cfg, "affinity", &set->affinity))
		set->affinity = 0;

	if (!config_setting_lookup_int(cfg, "priority", &set->priority))
		set->priority = 0;

	if (!config_setting_lookup_float(cfg, "stats", &set->stats))
		set->stats = 0;
	
	
	cfg_log = config_setting_get_member(cfg, "log");
	if (cfg_log)
		cfg_parse_log(cfg_log, set);
	
	cfg_plugins = config_setting_get_member(cfg, "plugins");
	if (cfg_plugins)
		cfg_parse_plugins(cfg_plugins);

	return 0;
}

void cfg_init(config_t *cfg)
{
	config_init(cfg);
}

void cfg_destroy(config_t *cfg)
{
	config_destroy(cfg);
}

int cfg_parse(const char *filename, config_t *cfg, struct settings *set,
	struct list *nodes, struct list *paths)
{
	int ret = CONFIG_FALSE;
	char *filename_cpy, *include_dir;

	config_init(cfg);

	filename_cpy = strdup(filename);
	include_dir = dirname(filename_cpy);

	/* Setup libconfig */
	config_set_auto_convert(cfg, 1);
	config_set_include_dir(cfg, include_dir);

	free(filename_cpy);
	
	if (strcmp("-", filename) == 0)
		ret = config_read(cfg, stdin);
	else if (access(filename, F_OK) != -1)
		ret = config_read_file(cfg, filename);
	else
		error("Invalid configuration file name: %s", filename);

	if (ret != CONFIG_TRUE) {
		error("Failed to parse configuration: %s in %s:%d",
			config_error_text(cfg),
			config_error_file(cfg) ? config_error_file(cfg) : filename,
			config_error_line(cfg)
		);
	}

	config_setting_t *cfg_root = config_root_setting(cfg);

	/* Parse global settings */
	if (set) {
		if (!cfg_root || !config_setting_is_group(cfg_root))
			error("Missing global section in config file: %s", filename);

		cfg_parse_global(cfg_root, set);
	}

	/* Parse nodes */
	if (nodes) {
		config_setting_t *cfg_nodes = config_setting_get_member(cfg_root, "nodes");
		if (!cfg_nodes || !config_setting_is_group(cfg_nodes))
			error("Missing node section in config file: %s", filename);

		for (int i = 0; i < config_setting_length(cfg_nodes); i++) {
			config_setting_t *cfg_node = config_setting_get_elem(cfg_nodes, i);
			
			struct node_type *vt;
			const char *type;

			/* Required settings */
			if (!config_setting_lookup_string(cfg_node, "type", &type))
				cerror(cfg_node, "Missing node type");
			
			vt = list_lookup(&node_types, type);
			if (!vt)
				cerror(cfg_node, "Invalid node type: %s", type);
			
			struct node *n = node_create(vt);

			ret = node_parse(n, cfg_node, set);
			if (ret)
				cerror(cfg_node, "Failed to parse node");
			
			list_push(nodes, n);
		}
	}

	/* Parse paths */
	if (paths) {
		config_setting_t *cfg_paths = config_setting_get_member(cfg_root, "paths");
		if (!cfg_paths || !config_setting_is_list(cfg_paths))
			error("Missing path section in config file: %s", filename);

		for (int i = 0; i < config_setting_length(cfg_paths); i++) {
			config_setting_t *cfg_path = config_setting_get_elem(cfg_paths, i);
			
			struct path *p = path_create();
			
			ret = path_parse(p, cfg_path, nodes, set);
			if (ret)
				cerror(cfg_path, "Failed to parse path");
			
			list_push(paths, p);

			if (p->reverse) {
				struct path *r = path_create();
		
				ret = path_reverse(p, r);
				if (ret)
					cerror(cfg_path, "Failed to reverse path %s", path_name(p));

				list_push(paths, r);
			}
		}
	}

	return 0;
}