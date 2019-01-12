/** Global configuration file for VILLASnode.
 *
 * The syntax of this file is similar to JSON.
 * A detailed description of the format can be found here:
 *   http://www.hyperrealm.com/libconfig/libconfig_manual.html//Configuration-Files
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

		htdocs : "/villas/web/socket/",		// Root directory of internal webserver
		port : 80				// Port for HTTP connections
	}
};