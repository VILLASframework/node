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
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *
 * @addtogroup hooks User-defined hook functions
 * @{
 *********************************************************************************/
 
#ifndef _HOOKS_H_
#define _HOOKS_H_

#include <time.h>

#define REGISTER_HOOK(name, prio, fnc, type)			\
__attribute__((constructor)) void __register_ ## fnc () {	\
	static struct hook h = { name, prio, fnc, type };	\
	list_push(&hooks, &h);					\
}

/* The configuration of hook parameters is done in "config.h" */

/* Forward declarations */
struct path;

extern struct list hooks;

/** Callback type of hook function
 *
 * @param p The path which is processing this message.
 * @retval 0 Success. Continue processing and forwarding the message.
 * @retval <0 Error. Drop the message.
 */
typedef int (*hook_cb_t)(struct path *p);

enum hook_type {
	HOOK_PATH_START,	/**< Called whenever a path is started; before threads are created. */
	HOOK_PATH_STOP,		/**< Called whenever a path is stopped; after threads are destoyed. */
	HOOK_PATH_RESTART,	/**< Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. */

	HOOK_PRE,		/**< Called when a new packet of messages (samples) was received. */
	HOOK_POST,		/**< Called after each message (sample) of a packet was processed. */
	HOOK_MSG,		/**< Called for each message (sample) in a packet. */
	
	HOOK_PERIODIC,		/**< Called periodically. Period is set by global 'stats' option in the configuration file. */

	HOOK_MAX
};

/** Descriptor for user defined hooks. See hook_list[]. */
struct hook {
	/** The unique name of this hook. This must be the first member! */
	const char *name;
	int priority;
	hook_cb_t callback;
	enum hook_type type;
};

struct hook * hook_lookup(const char *name);

/* The following prototypes are example hooks */

/** Example hook: Print the message.
 * @example
 */
int hook_print(struct path *p);

/** Example hook: Drop messages. */
int hook_decimate(struct path *p);

/** Example hook: Convert the message values to fixed precision. */
int hook_tofixed(struct path *p);

/** Example hook: overwrite timestamp of message. */
int hook_ts(struct path *p);

/** Example hook: Finite-Impulse-Response (FIR) filter. */
int hook_fir(struct path *p);

/** Example hook: Discrete Fourier Transform */
int hook_dft(struct path *p);

/* The following prototypes are core hook functions */

/** Core hook: verify message headers. Invalid messages will be dropped. */
int hook_verify(struct path *p);

/** Core hook: reset the path in case a new simulation was started. */
int hook_restart(struct path *p);

/** Core hook: check if sequence number is correct. Otherwise message will be dropped */
int hook_drop(struct path *p);

#endif /** _HOOKS_H_ @} */
