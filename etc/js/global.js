/** Global configuration file for VILLASnode.
 *
 * The syntax of this file is similar to JSON.
 * A detailed description of the format can be found here:
 *   http://www.hyperrealm.com/libconfig/libconfig_manual.html//Configuration-Files
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

os = require('os');

var logfile = "/var/log/villas-node_" + new Date().toISOString() + ".log"

module.exports = {
	affinity : 0x01,				// Mask of cores the server should run on
							// This also maps the NIC interrupts to those cores!

	priority : 50,					// Priority for the server tasks.
							// Usually the server is using a real-time FIFO
							// scheduling algorithm

							// See: https://github.com/docker/docker/issues/22380
							//  on why we cant use real-time scheduling in Docker

	stats : 3,					// The interval in seconds to print path statistics.
							// A value of 0 disables the statistics.

	name : os.hostname(),				// The name of this VILLASnode. Might by used by node-types
							// to identify themselves (default is the hostname).

	log : {
		level : 5,				// The level of verbosity for debug messages
							// Higher number => increased verbosity

		faciltities : [ "path", "socket" ],	// The list of enabled debug faciltities.
							// If omitted, all faciltities are enabled
							// For a full list of available faciltities, check lib/log.c

		file : logfile,				// File for logs
	},

	http : {
		enabled : true,				// Do not listen on port if true

		port : 80				// Port for HTTP connections
	}
};
