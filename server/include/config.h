/** Static server configuration
 *
 * This file contains some compiled-in settings.
 * This settings are not part of the configuration file.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#ifndef _GIT_REV
 #define _GIT_REV	"nogit"
#endif

/** The version number of the s2ss server */
#define VERSION		"v0.4-" _GIT_REV

/** Maximum number of float values in a message */
#define MAX_VALUES	16

/** Maximum number of messages in the circular history buffer */
#define POOL_SIZE	16

/** Width of log output in characters */
#define LOG_WIDTH	100

/** Socket priority */
#define SOCKET_PRIO	7

/* Protocol numbers */
#define IPPROTO_S2SS	137
#define ETH_P_S2SS	0xBABE

#endif /* _CONFIG_H_ */
