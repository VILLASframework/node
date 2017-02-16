/** Loadable / plugin support.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#pragma once

#include "hooks.h"
#include "api.h"
#include "fpga/ip.h"

#define REGISTER_PLUGIN(p)					\
__attribute__((constructor)) static void UNIQUE(__ctor)() {	\
	list_push(&plugins, p);					\
	(p)->load(p);						\
}								\
__attribute__((destructor)) static void UNIQUE(__dtor)() {	\
	(p)->unload(p);						\
	list_remove(&plugins, p);				\
}

extern struct list plugins;

enum plugin_types {
	PLUGIN_TYPE_HOOK,
	PLUGIN_TYPE_NODE,
	PLUGIN_TYPE_API,
	PLUGIN_TYPE_FPGA_IP,
	PLUGIN_TYPE_MODEL_CBUILDER
};

enum plugin_state {
	PLUGIN_STATE_DESTROYED,
	PLUGIN_STATE_UNLOADED,
	PLUGIN_STATE_LOADED
};

struct plugin {
	char *name;
	char *description;
	void *handle;
	
	enum plugin_types type;
	enum plugin_state state;
	
	int (*load)(struct plugin *p);
	int (*unload)(struct plugin *p);
	
	union {
		struct api_ressource api;
		struct hook hook;
		struct node_type node;
	};
};

struct plugin * plugin_lookup(char *name, enum plugin_types type);

int plugin_init(struct plugin *p, char *name, char *path);

int plugin_load(struct plugin *p);

int plugin_unload(struct plugin *p);

int plugin_destroy(struct plugin *p);
