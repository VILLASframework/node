/** Redis node-type
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

/**
 * @addtogroup redis BSD redis Node Type
 * @ingroup node
 * @{
 */

#pragma once

#include <atomic>
#include <thread>
#include <unordered_map>

#include <sw/redis++/redis++.h>

#include <villas/node/config.h>
#include <villas/node.h>
#include <villas/timing.h>
#include <villas/format.hpp>
#include <villas/task.hpp>
#include <villas/pool.h>
#include <villas/queue_signalled.h>

enum class RedisMode {
	KEY,
	HASH,
	CHANNEL
};

class RedisConnection {

public:
	sw::redis::Redis context;

protected:
	enum State {
		INITIALIZED,
		RUNNING,
		STOPPING
	};

	std::thread thread;
	std::atomic<enum State> state;

	void onMessage(const std::string &channel, const std::string &msg);

	void loop();

	std::unordered_multimap<std::string, struct vnode *> subscriberMap;

	sw::redis::Subscriber subscriber;

	villas::Logger logger;

public:

	RedisConnection(const sw::redis::ConnectionOptions &opts);

	static
	RedisConnection * get(const sw::redis::ConnectionOptions &opts);

	void start();
	void stop();

	void subscribe(struct vnode *n, const std::string &channel);
	void unsubscribe(struct vnode *n, const std::string &channel);
};

struct redis {
	sw::redis::ConnectionOptions options;

	RedisConnection *conn;

	enum RedisMode mode;

	std::string key;

	bool notify;			/**< Use Redis Keyspace notifications to listen for updates. */

	struct Task task;		/**< Timer for periodic events. */
	double rate;			/**< Rate for polling key updates if keyspace notifications are disabled. */

	villas::node::Format *formatter;

	struct pool pool;
	struct queue_signalled queue;
};

/** @see node_type::init */
int redis_init(struct vnode *n);

/** @see node_type::destroy */
int redis_destroy(struct vnode *n);

/** @see node_type::parse */
int redis_parse(struct vnode *n, json_t *json);

/** @see node_type::print */
char * redis_print(struct vnode *n);

/** @see node_type::check */
int redis_check();

/** @see node_type::prepare */
int redis_prepare();

/** @see node_type::start */
int redis_start(struct vnode *n);

/** @see node_type::stop */
int redis_stop(struct vnode *n);

/** @see node_type::pause */
int redis_pause(struct vnode *n);

/** @see node_type::resume */
int redis_resume(struct vnode *n);

/** @see node_type::write */
int redis_write(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::read */
int redis_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::reverse */
int redis_reverse(struct vnode *n);

/** @see node_type::poll_fds */
int redis_poll_fds(struct vnode *n, int fds[]);

/** @see node_type::netem_fds */
int redis_netem_fds(struct vnode *n, int fds[]);

/** @} */
