/** Hook funktions
 *
 * Every path can register a hook function which is called for every received
 * message. This can be used to debug the data flow, get statistics
 * or alter the message.
 *
 * This file includes some examples.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 */
/**
 * @addtogroup hooks User-defined hook functions
 * @ingroup path
 * @{
 *********************************************************************************/
 
#pragma once

#include <time.h>
#include <string.h>
#include <stdbool.h>

#include "queue.h"
#include "list.h"
#include "super_node.h"
#include "common.h"

/* Forward declarations */
struct path;
struct hook;
struct sample;
struct super_node;

struct hook_type {
	int priority;		/**< Default priority of this hook type. */
	bool builtin;		/**< Should we add this hook by default to every path?. */

	size_t size;		/**< Size of allocation for struct hook::_vd */
	
	int (*parse)(struct hook *h, config_setting_t *cfg);

	int (*init)(struct hook *h);	/**< Called before path is started to parseHOOK_DESTROYs. */
	int (*destroy)(struct hook *h);	/**< Called after path has been stopped to release memory allocated by HOOK_INIT */	

	int (*start)(struct hook *h);	/**< Called whenever a path is started; before threads are created. */
	int (*stop)(struct hook *h);	/**< Called whenever a path is stopped; after threads are destoyed. */
	
	int (*periodic)(struct hook *h);/**< Called periodically. Period is set by global 'stats' option in the configuration file. */
	int (*restart)(struct hook *h);	/**< Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. */

	int (*read)(struct hook *h, struct sample *smps[], size_t *cnt);	/**< Called for every single received samples. */
	int (*write)(struct hook *h, struct sample *smps[], size_t *cnt);	/**< Called for every single sample which will be sent. */
};

/** Descriptor for user defined hooks. See hooks[]. */
struct hook {
	enum state state;

	struct sample *prev, *last;
	struct path *path;
	
	struct hook_type *_vt;	/**< C++ like Vtable pointer. */
	void *_vd;		/**< Private data for this hook. This pointer can be used to pass data between consecutive calls of the callback. */
	
	int priority;		/**< A priority to change the order of execution within one type of hook. */
};

/** Save references to global nodes, paths and settings */
int hook_init(struct hook *h, struct hook_type *vt, struct path *p);

/** Parse a single hook.
 *
 * A hook definition is composed of the hook name and optional parameters
 * seperated by a colon.
 *
 * Examples:
 *   "print:stdout"
 */
int hook_parse(struct hook *h, config_setting_t *cfg);

int hook_destroy(struct hook *h);

int hook_start(struct hook *h);
int hook_stop(struct hook *h);

int hook_periodic(struct hook *h);
int hook_restart(struct hook *h);

int hook_read(struct hook *h, struct sample *smps[], size_t *cnt);
int hook_write(struct hook *h, struct sample *smps[], size_t *cnt);

size_t hook_read_list(struct list *hs, struct sample *smps[], size_t cnt);
size_t hook_write_list(struct list *hs, struct sample *smps[], size_t cnt);

/** Compare two hook functions with their priority. Used by list_sort() */
int hook_cmp_priority(const void *a, const void *b);

/** Parses an object of hooks
 *
 * Example:
 *
 * {
 *    stats = {
 *       output = "stdout"
 *    },
 *    skip_first = {
 *       seconds = 10
 *    },
 *    hooks = [ "print" ]
 * }
 */
int hook_parse_list(struct list *list, config_setting_t *cfg, struct path *p);