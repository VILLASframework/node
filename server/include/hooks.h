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
 */
/**
 * @addtogroup hooks User-defined hook functions
 * @ingroup path
 * @{
 *********************************************************************************/
 
#ifndef _HOOKS_H_
#define _HOOKS_H_

#include <time.h>

#define REGISTER_HOOK(name, prio, fnc, type)				\
__attribute__((constructor)) void __register_ ## fnc () {		\
	static struct hook h = { name, NULL, prio, type, NULL, fnc };	\
	list_push(&hooks, &h);						\
}

/* The configuration of hook parameters is done in "config.h" */

/* Forward declarations */
struct path;
struct hook;

/** This is a list of hooks which can be used in the configuration file. */
extern struct list hooks;

/** Callback type of hook function
 *
 * @param p The path which is processing this message.
 * @param h The hook datastructure which contains parameter, name and private context for the hook.
 * @param when Provides the type of hook for which this occurence of the callback function was executed. See hook_type for possible values.
 * @retval 0 Success. Continue processing and forwarding the message.
 * @retval <0 Error. Drop the message.
 */
typedef int (*hook_cb_t)(struct path *p, struct hook *h, int when);

/** The type of a hook defines when a hook will be exectuted. This is used as a bitmask. */
enum hook_type {
	HOOK_PATH_START		= 1 << 0, /**< Called whenever a path is started; before threads are created. */
	HOOK_PATH_STOP		= 1 << 1, /**< Called whenever a path is stopped; after threads are destoyed. */
	HOOK_PATH_RESTART	= 1 << 2, /**< Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. */

	HOOK_PRE		= 1 << 3, /**< Called when a new packet of messages (samples) was received. */
	HOOK_POST		= 1 << 4, /**< Called after each message (sample) of a packet was processed. */
	HOOK_MSG		= 1 << 5, /**< Called for each message (sample) in a packet. */
	HOOK_ASYNC		= 1 << 6, /**< Called asynchronously with fixed rate (see path::rate). */
	
	HOOK_PERIODIC		= 1 << 7, /**< Called periodically. Period is set by global 'stats' option in the configuration file. */
	
	HOOK_INIT		= 1 << 8, /**< Called to allocate and init hook-private data */
	HOOK_DEINIT		= 1 << 9, /**< Called to free hook-private data */
	
	/** @{ Classes of hooks */
	/** Internal hooks are mandatory. */
	HOOK_INTERNAL		= 1 << 16,
	/** Hooks which are using private data must allocate and free them propery. */	
	HOOK_PRIVATE		= HOOK_INIT | HOOK_DEINIT,
	/** All path related actions */
	HOOK_PATH		= HOOK_PATH_START | HOOK_PATH_STOP | HOOK_PATH_RESTART,
	/** Hooks which are used to collect statistics. */
	HOOK_STATS		= HOOK_INTERNAL | HOOK_PRIVATE | HOOK_PATH | HOOK_MSG | HOOK_PRE | HOOK_PERIODIC,
	/** All hooks */
	HOOK_ALL		= HOOK_INTERNAL - 1
	/** @} */
};

/** Descriptor for user defined hooks. See hooks[]. */
struct hook {
	const char *name;	/**< The unique name of this hook. This must be the first member! */
	const char *parameter;	/**< A parameter string for this hook. Can be used to configure the hook behaviour. */
	int priority;		/**< A priority to change the order of execution within one type of hook */
	enum hook_type type;	/**< The type of the hook as a bitfield */
	void *private;		/**< Private data for this hook. This pointer can be used to pass data between consecutive calls of the callback. */
	hook_cb_t cb;		/**< The hook callback function as a function pointer. */
};

/** The following prototypes are example hooks
 *
 * @addtogroup hooks_examples Examples for hook functions
 * @{
 */

/** Example hook: Print the message. */
int hook_print(struct path *p, struct hook *h, int when);

/** Example hook: Drop messages. */
int hook_decimate(struct path *p, struct hook *h, int when);

/** Example hook: Convert the values of a message between fixed (integer) and floating point representation. */
int hook_convert(struct path *p, struct hook *h, int when);

/** Example hook: overwrite timestamp of message. */
int hook_ts(struct path *p, struct hook *h, int when);

/** Example hook: Finite-Impulse-Response (FIR) filter. */
int hook_fir(struct path *p, struct hook *h, int when);

/** Example hook: drop first samples after simulation restart */
int hook_skip_first(struct path *p, struct hook *h, int when);

/** Example hook: Skip messages whose values are similiar to the previous ones */
int hook_skip_unchanged(struct path *p, struct hook *h, int when);

/* The following hooks are used to implement core functionality */

/** Core hook: verify message headers. Invalid messages will be dropped. */
int hook_verify(struct path *p, struct hook *h, int when);

/** Core hook: reset the path in case a new simulation was started. */
int hook_restart(struct path *p, struct hook *h, int when);

/** Core hook: check if sequence number is correct. Otherwise message will be dropped */
int hook_drop(struct path *p, struct hook *h, int when);

/** Core hook: collect statistics */
int hook_stats(struct path *p, struct hook *h, int when);

/** Core hook: send path statistics to another node */
int hook_stats_send(struct path *p, struct hook *h, int when);

/** Not a hook: just prints header for periodic statistics */
void hook_stats_header();

#endif /** _HOOKS_H_ @} @} */
