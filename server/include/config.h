/** Static server configuration
 *
 * This file contains some compiled-in settings.
 * This settings are not part of the configuration file.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

/** The version number of the s2ss server */
#define VERSION		"v0.4" __GIT_REV__

/** Maximum number of double values in a struct msg */
#define MAX_VALUES	16

/** Socket priority */
#define SOCKET_PRIO	7

/* Some parameters for histogram statistics */
#define HIST_SEQ	33

#endif /* _CONFIG_H_ */
