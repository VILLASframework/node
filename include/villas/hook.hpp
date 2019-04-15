/** Hook functions
 *
 * Every node or path can register hook functions which is called for every
 * processed sample. This can be used to debug the data flow, get statistics
 * or alter the sample contents.
 *
 * @file
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

#pragma once

#include <villas/hook.h>
#include <villas/list.h>
#include <villas/signal.h>
#include <villas/log.hpp>
#include <villas/plugin.hpp>
#include <villas/exceptions.hpp>

/* Forward declarations */
struct path;
struct node;
struct sample;

namespace villas {
namespace node {

class Hook {

protected:
	Logger logger;

	enum state state;

	int flags;
	int priority;		/**< A priority to change the order of execution within one type of hook. */
	int enabled;		/**< Is this hook active? */

	struct path *path;
	struct node *node;

	vlist signals;

	json_t *cfg;		/**< A JSON object containing the configuration of the hook. */

public:
	Hook(struct path *p, struct node *n, int fl, int prio, bool en = true);
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
		assert(state == STATE_PREPARED);

		state = STATE_STARTED;
	}

	/** Called whenever a hook is stopped; after threads are destoyed. */
	virtual void stop()
	{
		assert(state == STATE_STARTED);

		state = STATE_STOPPED;
	}

	virtual void check()
	{
		assert(state == STATE_PARSED);

		state = STATE_CHECKED;
	}

	/** Called periodically. Period is set by global 'stats' option in the configuration file. */
	virtual void periodic()
	{
		assert(state == STATE_STARTED);
	}

	/** Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. */
	virtual void restart()
	{
		assert(state == STATE_STARTED);
	}

	/** Called whenever a sample is processed. */
	virtual int process(sample *smp)
	{
		return HOOK_OK;
	};

	int getPriority() const
	{
		return priority;
	}

	int getFlags() const
	{
		return flags;
	}

	struct vlist * getSignals()
	{
		return &signals;
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
		assert(state == STATE_INITIALIZED);

		state = STATE_PARSED;
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

protected:
	int flags;
	int priority;

public:
	HookFactory(const std::string &name, const std::string &desc, int fl, int prio) :
		Plugin(name, desc),
		flags(fl),
		priority(prio)
	{ }

	virtual Hook * make(struct path *p, struct node *n) = 0;

	int getFlags()
	{
		return flags;
	}
};

template<typename T>
class HookPlugin : public HookFactory {

public:
	using HookFactory::HookFactory;

	virtual Hook * make(struct path *p, struct node *n) {
		return new T(p, n, flags, priority);
	};
};

} // node
} // villas
