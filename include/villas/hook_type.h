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
 */
/**
 * @addtogroup hooks User-defined hook functions
 * @ingroup path
 * @{
 *********************************************************************************/

#pragma once

#include <stdlib.h>
#include <stdbool.h>

#include <jansson.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct hook;
struct sample;

enum hook_flags {
	HOOK_BUILTIN	= (1 << 0), /**< Should we add this hook by default to every path?. */
	HOOK_PATH	= (1 << 1), /**< This hook type is used by paths. */
	HOOK_NODE	= (1 << 2)  /**< This hook type is used by nodes. */
};

struct hook_type {
	int priority;		/**< Default priority of this hook type. */
	int flags;

	size_t size;		/**< Size of allocation for struct hook::_vd */

	int (*parse)(struct hook *h, json_t *cfg);
	int (*parse_cli)(struct hook *h, int argc, char *argv[]);

	int (*init)(struct hook *h);	/**< Called before path is started to parseHOOK_DESTROYs. */
	int (*destroy)(struct hook *h);	/**< Called after path has been stopped to release memory allocated by HOOK_INIT */

	int (*start)(struct hook *h);	/**< Called whenever a path is started; before threads are created. */
	int (*stop)(struct hook *h);	/**< Called whenever a path is stopped; after threads are destoyed. */

	int (*periodic)(struct hook *h);/**< Called periodically. Period is set by global 'stats' option in the configuration file. */
	int (*restart)(struct hook *h);	/**< Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. */

	int (*read)(struct hook *h, struct sample *smps[], unsigned *cnt);	/**< Called whenever samples have been read from a node. */
	int (*process)(struct hook *h, struct sample *smps[], unsigned *cnt);	/**< Called whenever muxed samples are processed. */
	int (*write)(struct hook *h, struct sample *smps[], unsigned *cnt);	/**< Called whenever samples are written to a node. */
};

struct hook_type * hook_type_lookup(const char *name);

#ifdef __cplusplus
}
#endif