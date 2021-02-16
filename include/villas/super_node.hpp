/** The super node object holding the state of the application.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/node/config.h>

#ifdef WITH_GRAPHVIZ
extern "C" {
	#include <graphviz/gvc.h>
}
#endif

#include <fstream>

#include <villas/list.h>
#include <villas/api.hpp>
#include <villas/web.hpp>
#include <villas/log.hpp>
#include <villas/config.hpp>
#include <villas/node.h>
#include <villas/task.hpp>
#include <villas/common.hpp>
#include <villas/kernel/if.hpp>

/* Forward declarations */
struct vnode;

namespace villas {
namespace node {

/** Global configuration */
class SuperNode {

protected:
	enum State state;

	int idleStop;

	Logger logger;

	struct vlist nodes;
	struct vlist paths;
	std::list<kernel::Interface *> interfaces;

#ifdef WITH_API
	Api api;
#endif

#ifdef WITH_WEB
	Web web;
#endif

	int priority;		/**< Process priority (lower is better) */
	int affinity;		/**< Process affinity of the server and all created threads */
	int hugepages;		/**< Number of hugepages to reserve. */
	double statsRate;	/**< Rate at which we display the periodic stats. */

	struct Task task;	/**< Task for periodic stats output */

	uuid_t uuid;		/**< A globally unique identifier of the instance */

	struct timespec started;	/**< The time at which the instance has been started. */

	std::string uri;	/**< URI of configuration */

	Config config;		/** The configuration file. */

public:
	/** Inititalize configuration object before parsing the configuration. */
	SuperNode();

	int init();

	/** Wrapper for parse() which loads the config first. */
	void parse(const std::string &name);

	/** Parse super-node configuration.
	 *
	 * @param json A libjansson object which contains the configuration.
	 */
	void parse(json_t *json);

	/** Check validity of super node configuration. */
	void check();

	/** Initialize after parsing the configuration file. */
	void prepare();
	void start();
	void stop();
	void run();

	void preparePaths();
	void prepareNodes();

	void startPaths();
	void startNodes();
	void startNodeTypes();
	void startInterfaces();

	void stopPaths();
	void stopNodes();
	void stopNodeTypes();
	void stopInterfaces();

#ifdef WITH_GRAPHVIZ
	graph_t * getGraph();
#endif

	/** Run periodic hooks of this super node. */
	int periodic();

	void setState(enum State st)
	{
		state = st;
	}

	struct vnode * getNode(const std::string &name)
	{
		return vlist_lookup_name<struct vnode>(&nodes, name);
	}

	struct vlist * getNodes()
	{
		return &nodes;
	}

	struct vlist * getPaths()
	{
		return &paths;
	}

	std::list<kernel::Interface *> & getInterfaces()
	{
		return interfaces;
	}

	enum State getState()  const
	{
		return state;
	}

	void getUUID(uuid_t out) const
	{
		uuid_copy(out, uuid);
	}

	struct timespec getStartTime() const
	{
		return started;
	}

#ifdef WITH_API
	Api * getApi()
	{
		return &api;
	}
#endif

#ifdef WITH_WEB
	Web * getWeb()
	{
		return &web;
	}
#endif

	json_t * getConfig()
	{
		return config.root;
	}

	std::string getConfigUri() const
	{
		return uri;
	}

	int getAffinity() const
	{
		return affinity;
	}

	Logger getLogger()
	{
		return logger;
	}

	/** Destroy configuration object. */
	~SuperNode();
};

} /* namespace node */
} /* namespace villas */
