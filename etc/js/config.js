/** Example Javascript config
 *
 * This example demonstrates how you can use Javascript code and NodeJS
 * to script configuration files.
 *
 * To use this configuration, run the following command:
 *
 *    villas node <(node /etc/villas/node/js/config.js)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

var glob   = require('glob');
var global = require(__dirname + '/global.js')

/* List of plugins
 *
 * Additional node-types, hooks or VILLASfpga IP cores
 * can be loaded by compiling them into a shared library and
 * adding them to this list
 */
global.plugins = glob.sync('/usr?(/local)/share/villas/node/plugins/*.so');

global.nodes = {
	loopback_node : {
		vectorize : 1,
		type : "loopback",	// A loopback node will receive exactly the same data which has been sent to it.
					// The internal implementation is based on queue.
		queuelen : 10240	// The queue length of the internal queue which buffers the samples.
	},
	socket_node : {
		type : "socket",

		local : "*:12000",
		remote : "127.0.0.1:12000"
	}
};

global.paths = [
	{
		in : "test_node",
		out : "socket_node",
		queuelen : 10000
	},
	{
		in : "socket_node",
		out : "test_node",
		queuelen : 10000,
		hooks : [
			{
				type : "stats",
				warmup : 100,
				verbose : true,
				format : "human",
				output : "./stats.log"
			},
			{
				type : "convert"
			}
		]
	}
];

// Convert Javascript Object to JSON string
var json = JSON.stringify(global, null, 4);

// Some log message
process.stderr.write('Configuration file successfully generated\n');

// Print JSON to stdout
process.stdout.write(json);