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

#ifndef _GIT_REV
 #define _GIT_REV	"nogit"
#endif

/** The version number of the s2ss server */
#define VERSION		"v0.4-" _GIT_REV

/** Maximum number of double values in a struct msg */
#define MAX_VALUES	64

/** Socket priority */
#define SOCKET_PRIO	7

/* Protocol numbers */
#define IPPROTO_S2SS	137
#define ETH_P_S2SS	0xBABE

#endif /* _CONFIG_H_ */
