/** Statistic hooks.
 *
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

/** @addtogroup hooks Hook functions
 * @{
 */

#include <memory>

#include <villas/common.hpp>
#include <villas/hook.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/stats.hpp>
#include <villas/node.h>
#include <villas/timing.h>

namespace villas {
namespace node {

class StatsHook;

class StatsWriteHook : public Hook {

protected:
	StatsHook *parent;

public:
	StatsWriteHook(StatsHook *pa, struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		parent(pa)
	{
		state = State::CHECKED;
	}

	virtual Hook::Reason process(sample *smp);
};

class StatsReadHook : public Hook {

protected:
	sample *last;

	StatsHook *parent;

public:
	StatsReadHook(StatsHook *pa, struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		last(nullptr),
		parent(pa)
	{
		state = State::CHECKED;
	}

	virtual void start()
	{
		assert(state == State::PREPARED);

		last = nullptr;

		state = State::STARTED;
	}

	virtual void stop()
	{
		assert(state == State::STARTED);

		if (last)
			sample_decref(last);

		state = State::STOPPED;
	}

	virtual Hook::Reason process(sample *smp);
};

class StatsHook : public Hook {

	friend StatsReadHook;
	friend StatsWriteHook;

protected:
	StatsReadHook *readHook;
	StatsWriteHook *writeHook;

	enum Stats::Format format;
	int verbose;
	int warmup;
	int buckets;

	std::shared_ptr<Stats> stats;

	FILE *output;
	std::string uri;

public:

	StatsHook(struct vpath *p, struct vnode *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		format(Stats::Format::HUMAN),
		verbose(0),
		warmup(500),
		buckets(20),
		output(nullptr),
		uri()
	{
		readHook = new StatsReadHook(this, p, n, fl, prio, en);
		writeHook = new StatsWriteHook(this, p, n, fl, prio, en);

		if (!readHook || !writeHook)
			throw MemoryAllocationError();

		/* Add child hooks */
		if (node) {
			vlist_push(&node->in.hooks, (void *) readHook);
			vlist_push(&node->out.hooks, (void *) writeHook);
		}
	}

	StatsHook & operator=(const StatsHook&) = delete;
	StatsHook(const StatsHook&) = delete;

	virtual void start()
	{
		assert(state == State::PREPARED);

		if (!uri.empty()) {
			output = fopen(uri.c_str(), "w+");
			if (!output)
				throw RuntimeError("Failed to open file '{}' for writing", uri);
		}

		state = State::STARTED;
	}

	virtual void stop()
	{
		assert(state == State::STARTED);

		stats->print(uri.empty() ? stdout : output, format, verbose);

		if (!uri.empty())
			fclose(output);

		state = State::STOPPED;
	}

	virtual void restart()
	{
		assert(state == State::STARTED);

		stats->reset();
	}

	virtual Hook::Reason process(sample *smp)
	{
		// Only call readHook if it hasnt been added to the node's hook list
		if (!node)
			return readHook->process(smp);

		return Hook::Reason::OK;
	}

	virtual void periodic()
	{
		assert(state == State::STARTED);

		stats->printPeriodic(uri.empty() ? stdout : output, format, node);
	}

	virtual void parse(json_t *json)
	{
		int ret;
		json_error_t err;

		assert(state != State::STARTED);

		Hook::parse(json);

		const char *f = nullptr;
		const char *u = nullptr;

		ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: b, s?: i, s?: i, s?: s }",
			"format", &f,
			"verbose", &verbose,
			"warmup", &warmup,
			"buckets", &buckets,
			"output", &u
		);
		if (ret)
			throw ConfigError(json, err, "node-config-hook-stats");

		if (f) {
			try {
				format = Stats::lookupFormat(f);
			} catch (const std::invalid_argument &e) {
				throw ConfigError(json, "node-config-hook-stats", "Invalid statistic output format: {}", f);
			}
		}

		if (u)
			uri = u;

		state = State::PARSED;
	}

	virtual void prepare()
	{
		assert(state == State::CHECKED);

		stats = std::make_shared<villas::Stats>(buckets, warmup);

		/* Register statistic object to node.
		*
		* This allows the node code to update statistics. */
		if (node)
			node->stats = stats;

		state = State::PREPARED;
	}
};

Hook::Reason StatsWriteHook::process(sample *smp)
{
	timespec now = time_now();

	parent->stats->update(Stats::Metric::AGE, time_delta(&smp->ts.received, &now));

	return Reason::OK;
}

Hook::Reason StatsReadHook::process(sample *smp)
{
	if (last) {
		if (smp->flags & last->flags & (int) SampleFlags::HAS_TS_RECEIVED)
			parent->stats->update(Stats::Metric::GAP_RECEIVED, time_delta(&last->ts.received, &smp->ts.received));

		if (smp->flags & last->flags & (int) SampleFlags::HAS_TS_ORIGIN)
			parent->stats->update(Stats::Metric::GAP_SAMPLE, time_delta(&last->ts.origin, &smp->ts.origin));

		if ((smp->flags & (int) SampleFlags::HAS_TS_ORIGIN) && (smp->flags & (int) SampleFlags::HAS_TS_RECEIVED))
			parent->stats->update(Stats::Metric::OWD, time_delta(&smp->ts.origin, &smp->ts.received));

		if (smp->flags & last->flags & (int) SampleFlags::HAS_SEQUENCE) {
			int dist = smp->sequence - (int32_t) last->sequence;
			if (dist != 1)
				parent->stats->update(Stats::Metric::SMPS_REORDERED, dist);
		}
	}

	sample_incref(smp);

	if (last)
		sample_decref(last);

	last = smp;

	return Reason::OK;
}

/* Register hook */
static char n[] = "stats";
static char d[] = "Collect statistics for the current node";
static HookPlugin<StatsHook, n, d, (int) Hook::Flags::NODE_READ> p;

} /* namespace node */
} /* namespace villas */

/** @} */

