/** Hook funktions
 *
 * Every path can register a hook function which is called for every received
 * message. This can be used to debug the data flow, get statistics
 * or alter the message.
 *
 * This file includes some examples.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 * @file
 * @addtogroup hooks User-defined hook functions
 * @{
 */
 
#ifndef _HOOKS_H_
#define _HOOKS_H_

/* The configuration of hook parameters is done in "config.h" */

struct msg;
struct path;

/** Callback type of hook function
 *
 * @param m The last message which has been received
 * @param p The path which is processing this message.
 * @retval 0 Success. Continue processing and forwarding the message.
 * @retval <0 Error. Drop the message.
 */
typedef int (*hook_cb_t)(struct msg *m, struct path *p);

/** This is a static list of available hooks.
 *
 * It's used by hook_lookup to parse hook identfiers from the configuration file.
 * The list must be terminated by NULL pointers!
 */ 
struct hook_id {
	hook_cb_t cb;
	const char *name;
};

/** Get a function pointer of a hook function by its name
 *
 * @param name The name of the requested hook
 * @retval NULL There is no hook registred with name.
 * @retval >0 A function pointer to the requested hook_cb_t hook.
 */
hook_cb_t hook_lookup(const char *name);

/** Example hook: Print the message. */
int hook_print(struct msg *m, struct path *p);

/** Example hook: Drop messages. */
int hook_decimate(struct msg *m, struct path *p);

/** Example hook: Convert the message values to fixed precision. */
int hook_tofixed(struct msg *m, struct path *p);

/** Example hook: add timestamp to message. */
int hook_ts(struct msg *m, struct path *p);

/** Example hook: Finite-Impulse-Response (FIR) filter. */
int hook_fir(struct msg *m, struct path *p);

/** Example hook: Discrete Fourier Transform */
int hook_dft(struct msg *m, struct path *p);

#endif /** _HOOKS_H_ @} */
