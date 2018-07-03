/** The super node object holding the state of the application.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <villas/list.h>
#include <villas/api.h>
#include <villas/web.h>
#include <villas/log.h>
#include <villas/node.h>
#include <villas/common.h>

namespace villas {
namespace node {

/** Global configuration */
class SuperNode {

protected:
	enum state state;

	int priority;		/**< Process priority (lower is better) */
	int affinity;		/**< Process affinity of the server and all created threads */
	int hugepages;		/**< Number of hugepages to reserve. */
	double stats;		/**< Interval for path statistics. Set to 0 to disable them. */

	struct list nodes;
	struct list paths;
	struct list plugins;

	struct log log;
	struct api api;
	struct web web;

	char *name;		/**< A name of this super node. Usually the hostname. */

	char *uri;		/**< URI of configuration */

	json_t *json;		/**< JSON representation of the configuration. */

public:
	/** Inititalize configuration object before parsing the configuration. */
	SuperNode();

	int init();

	/** Wrapper for super_node_parse() */
	int parseUri(const char *uri);

	/** Parse super-node configuration.
	 *
	 * @param cfg A libjansson object which contains the configuration.
	 * @retval 0 Success. Everything went well.
	 * @retval <0 Error. Something went wrong.
	 */
	int parseJson(json_t *cfg);

	/** Check validity of super node configuration. */
	int check();

	/** Initialize after parsing the configuration file. */
	int start();
	int stop();
	void run();

	/** Run periodic hooks of this super node. */
	int periodic();

	struct node * getNode(const char *name) { return (struct node *) list_lookup(&nodes, name); }

	struct list * getNodes() { return &nodes; }
	struct list * getPaths() { return &paths; }
	struct web * getWeb() { return &web; }
	struct api * getApi() { return &api; }
	struct log * getLog() { return &log; }

	/** Desctroy configuration object. */
	~SuperNode();
};

} // node
} // villas
