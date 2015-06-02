/** Static server configuration
 *
 * This file contains some compiled-in settings.
 * This settings are not part of the configuration file.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _CONFIG_H_
#define _CONFIG_H_

#ifndef _GIT_REV
 #define _GIT_REV		"nogit"
#endif

/** The version number of the s2ss server */
#define VERSION			"v0.4-" _GIT_REV

/** Maximum number of float values in a message */
#define MAX_VALUES		16

/** Maximum number of messages in the circular history buffer */
#define DEFAULT_POOLSIZE	32

/** Width of log output in characters */
#define LOG_WIDTH		100

/** Socket priority */
#define SOCKET_PRIO		7

/* Protocol numbers */
#define IPPROTO_S2SS		137
#define ETH_P_S2SS		0xBABE

/* Hook function configuration */
#define HOOK_FIR_INDEX		 1 /**< The first value of message should be filtered. */
#define HOOK_TS_INDEX		-1 /**< The last value of message should be overwritten by a timestamp. */
#define HOOK_DECIMATE_RATIO 	30 /**< Only forward every 30th message to the destination nodes. */

/** Global configuration */
struct settings {
	/** Process priority (lower is better) */
	int priority;
	/** Process affinity of the server and all created threads */
	int affinity;
	/** Debug log level */
	int debug;
	/** Interval for path statistics. Set to 0 to disable themo disable them. */
	double stats;
};

#endif /* _CONFIG_H_ */
