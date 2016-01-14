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
#define VERSION			"v0.5-" _GIT_REV

/** Default number of values in a message */
#define DEFAULT_MSGVALUES	64

/** Maximum number of messages in the circular history buffer */
#define DEFAULT_POOLSIZE	32

/** Width of log output in characters */
#define LOG_WIDTH		132

/** Socket priority */
#define SOCKET_PRIO		7

/* Protocol numbers */
#define IPPROTO_S2SS		137
#define ETH_P_S2SS		0xBABE

#define SYSFS_PATH		"/sys"
#define PROCFS_PATH		"/proc"

/* Required kernel version */
#define KERNEL_VERSION_MAJ	3
#define KERNEL_VERSION_MIN	4
	
/** Coefficients for simple FIR-LowPass:
 *   F_s = 1kHz, F_pass = 100 Hz, F_block = 300
 *
 * Tip: Use MATLAB's filter design tool and export coefficients
 *      with the integrated C-Header export
 */
#define HOOK_FIR_COEFFS		{ -0.003658148158728, -0.008882653268281, 0.008001024183003,	\
				  0.08090485991761,    0.2035239551043,   0.3040703593515,	\
				  0.3040703593515,     0.2035239551043,   0.08090485991761,	\
				  0.008001024183003,  -0.008882653268281,-0.003658148158728 }
	
/** Global configuration */
struct settings {
	int priority;		/**< Process priority (lower is better) */
	int affinity;		/**< Process affinity of the server and all created threads */
	int debug;		/**< Debug log level */
	double stats;		/**< Interval for path statistics. Set to 0 to disable themo disable them. */
};

#endif /* _CONFIG_H_ */
