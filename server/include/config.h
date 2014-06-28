/** Static server configuration
 *
 * This file contains some defines which are not part of the configuration file.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

/** The version number of the s2ss server */
#define VERSION		__GIT_TAG__ "-" __GIT_REV__

/** Maximum number of double values in a struct msg */
#define MAX_VALUES	6

/** Maximum number of registrable hook functions per path */
#define MAX_HOOKS	5

/** Socket priority */
#define SOCKET_PRIO	7

#endif /* _CONFIG_H_ */
