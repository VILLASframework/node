/** Hook functions
 *
 * Every node or path can register hook functions which is called for every
 * processed sample. This can be used to debug the data flow, get statistics
 * or alter the sample contents.
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

#include <villas/list.h>
#include <villas/signal.h>
#include <villas/log.hpp>
#include <villas/plugin.hpp>
#include <villas/exceptions.hpp>

/* Forward declarations */
struct vpath;
struct vnode;
struct sample;

namespace villas {
namespace node {

class Hook {

public:
	enum class Flags {
		BUILTIN = (1 << 0),   /**< Should we add this hook by default to every path?. */
		PATH = (1 << 1),      /**< This hook type is used by paths. */
		NODE_READ = (1 << 2), /**< This hook type is used by nodes. */
		NODE_WRITE = (1 << 3) /**< This hook type is used by nodes. */
	};

	enum class Reason {
		OK,
		ERROR,
		SKIP_SAMPLE,
		STOP_PROCESSING
	};

protected:
	Logger logger;

	enum State state;

	int flags;
	int priority; /**< A priority to change the order of execution within one type of hook. */
	int enabled;  /**< Is this hook active? */

	struct vpath *path;
	struct vnode *node;

	struct vlist signals;

	json_t *cfg; /**< A JSON object containing the configuration of the hook. */

public:
	Hook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true);
	virtual ~Hook();

	virtual void parse(json_t *c);
	virtual void prepare();

	void prepare(struct vlist *sigs)
	{
		int ret;

		ret = signal_list_copy(&signals, sigs);
		if (ret)
			throw RuntimeError("Failed to copy signal list");

		prepare();
	}

	/** Called whenever a hook is started; before threads are created. */
	virtual void start()
	{
		assert(state == State::PREPARED);

		state = State::STARTED;
	}

	/** Called whenever a hook is stopped; after threads are destoyed. */
	virtual void stop()
	{
		assert(state == State::STARTED);

		state = State::STOPPED;
	}

	virtual void check()
	{
		assert(state == State::PARSED);

		state = State::CHECKED;
	}

	/** Called periodically. Period is set by global 'stats' option in the configuration file. */
	virtual void periodic()
	{
		assert(state == State::STARTED);
	}

	/** Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. */
	virtual void restart()
	{
		assert(state == State::STARTED);
	}

	/** Called whenever a sample is processed. */
	virtual Reason process(sample *smp)
	{
		return Reason::OK;
	};

	int getPriority() const
	{
		return priority;
	}

	int getFlags() const
	{
		return flags;
	}

	struct vlist *getSignals()
	{
		return &signals;
	}

	json_t * getConfig() const
	{
		return cfg;
	}

	bool isEnabled() const
	{
		return enabled;
	}
};

class LimitHook : public Hook {

public:
	using Hook::Hook;

	virtual void setRate(double rate, double maxRate = -1) = 0;

	void parse()
	{
		assert(state == State::INITIALIZED);

		state = State::PARSED;
	}

	void init()
	{
		parse();
		check();
		prepare();
		start();
	}
};

class HookFactory : public plugin::Plugin {

public:
	using plugin::Plugin::Plugin;

	virtual Hook *make(struct vpath *p, struct vnode *n) = 0;

	virtual int getFlags() const = 0;
	virtual int getPriority() const = 0;
};

template <typename T, const char *name, const char *desc, int flags = 0, int prio = 99>
class HookPlugin : public HookFactory {

public:
	using HookFactory::HookFactory;

	virtual Hook *make(struct vpath *p, struct vnode *n)
	{
		return new T(p, n, getFlags(), getPriority());
	}

	/// Get plugin name
	virtual std::string
	getName() const
	{ return name; }

	/// Get plugin description
	virtual std::string
	getDescription() const
	{ return desc; }

	/// Get hook flags
	virtual int
	getFlags() const
	{ return flags; }

	/// Get hook priority
	virtual int
	getPriority() const
	{ return prio; }
};

} /* namespace node */
} /* namespace villas */
