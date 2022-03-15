/** Hook functions
 *
 * Every node or path can register hook functions which is called for every
 * processed sample. This can be used to debug the data flow, get statistics
 * or alter the sample contents.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/list.hpp>
#include <villas/signal.hpp>
#include <villas/signal_list.hpp>
#include <villas/log.hpp>
#include <villas/plugin.hpp>
#include <villas/exceptions.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class Node;
class Path;
struct Sample;
class HookFactory;

class Hook {

	friend HookFactory;

public:
	using Ptr = std::shared_ptr<Hook>;

	enum class Flags {
		BUILTIN = (1 << 0),	/**< Should we add this hook by default to every path?. */
		PATH = (1 << 1),	/**< This hook type is used by paths. */
		NODE_READ = (1 << 2),	/**< This hook type is used by nodes. */
		NODE_WRITE = (1 << 3)	/**< This hook type is used by nodes. */
	};

	enum class Reason {
		OK = 0,
		ERROR,
		SKIP_SAMPLE,
		STOP_PROCESSING
	};

protected:
	Logger logger;

	HookFactory *factory;

	enum State state;

	int flags;
	unsigned priority;		/**< A priority to change the order of execution within one type of hook. */
	bool enabled;			/**< Is this hook active? */

	Path *path;
	Node *node;

	SignalList::Ptr signals;

	json_t *config;			/**< A JSON object containing the configuration of the hook. */

public:
	Hook(Path *p, Node *n, int fl, int prio, bool en = true);

	virtual
	~Hook()
	{ }

	virtual
	void parse(json_t *json);

	void prepare(SignalList::Ptr sigs);

	Logger getLogger()
	{
		return logger;
	}

	/** Called whenever a hook is started; before threads are created. */
	virtual
	void start()
	{
		assert(state == State::PREPARED);

		state = State::STARTED;
	}

	/** Called whenever a hook is stopped; after threads are destoyed. */
	virtual
	void stop()
	{
		assert(state == State::STARTED);

		state = State::STOPPED;
	}

	virtual
	void check()
	{
		assert(state == State::PARSED);

		state = State::CHECKED;
	}

	virtual
	void prepare()
	{
		assert(state == State::CHECKED);

		state = State::PREPARED;
	}

	/** Called periodically. Period is set by global 'stats' option in the configuration file. */
	virtual
	void periodic()
	{
		assert(state == State::STARTED);
	}

	/** Called whenever a new simulation case is started. This is detected by a sequence no equal to zero. */
	virtual
	void restart()
	{
		assert(state == State::STARTED);
	}

	/** Called whenever a sample is processed. */
	virtual
	Reason process(struct Sample *smp)
	{
		return Reason::OK;
	};

	unsigned getPriority() const
	{
		return priority;
	}

	int getFlags() const
	{
		return flags;
	}

	virtual
	SignalList::Ptr getSignals() const
	{
		return signals;
	}

	json_t * getConfig() const
	{
		return config;
	}

	HookFactory * getFactory() const
	{
		return factory;
	}

	bool isEnabled() const
	{
		return enabled;
	}
};

class SingleSignalHook : public Hook {

protected:
	unsigned signalIndex;
	std::string signalName;

public:
	SingleSignalHook(Path *p, Node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		signalIndex(0)
	{ }

	virtual
	void parse(json_t *json);

	virtual
	void prepare();
};

class MultiSignalHook : public Hook {

protected:
	std::list<unsigned> signalIndices;
	std::vector<std::string> signalNames;

public:
	using Hook::Hook;

	virtual
	void parse(json_t *json);

	virtual
	void prepare();

	virtual
	void check();
};

class LimitHook : public Hook {

public:
	using Ptr = std::shared_ptr<LimitHook>;
	using Hook::Hook;

	virtual void setRate(double rate, double maxRate = -1) = 0;

	virtual void parse()
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

protected:
	virtual
	void init(Hook::Ptr h)
	{
		h->logger = getLogger();
		h->factory = this;
	}

public:
	using plugin::Plugin::Plugin;

	virtual Hook::Ptr make(Path *p, Node *n) = 0;

	virtual
	int getFlags() const = 0;

	virtual
	unsigned getPriority() const = 0;

	virtual
	std::string getType() const
	{
		return "hook";
	}

	virtual
	bool isHidden() const
	{
		return false;
	}
};

template <typename T, const char *name, const char *desc, int flags = 0, unsigned prio = 99>
class HookPlugin : public HookFactory {

public:
	using HookFactory::HookFactory;

	virtual Hook::Ptr make(Path *p, Node *n)
	{
		auto h = std::make_shared<T>(p, n, getFlags(), getPriority());

		init(h);

		return h;
	}

	virtual
	std::string getName() const
	{
		return name;
	}

	virtual
	std::string getDescription() const
	{
		return desc;
	}

	virtual
	int getFlags() const
	{
		return flags;
	}

	virtual
	unsigned getPriority() const
	{
		return prio;
	}
};

} /* namespace node */
} /* namespace villas */
