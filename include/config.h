/**
 * Configuration
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

/// The version number of the s2ss server
#define VERSION		__GIT_TAG__ "-" __GIT_REV__
/// The Procotol version determines the format of a struct msg
#define PROTOCOL	0
/// The name of this node
#define NAME		"acs"

#define PRIORITY	sched_get_priority_max(SCHED_FIFO)
#define CORES		((1 << 6) | (1 << 7))

/// Maximum number of double values in a struct msg
#define MAX_VALUES	4
/// Maximum number of registrable hook functions per path
#define MAX_HOOKS	5
/// Maximum number of paths
#define MAX_PATHS	2
/// Maximum number of nodes
#define MAX_NODES	2
/// Size of the stack which gets prefaulted during initialization
#define MAX_SAFE_STACK	(16*1024)	/* 16 KiB */

#endif /* _CONFIG_H_ */
