/** Statistic hooks.
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

/** @addtogroup hooks Hook functions
 * @{
 */

#include <string.h>

#include <villas/common.h>
#include <villas/advio.h>
#include <villas/hook.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/stats.h>
#include <villas/node.h>
#include <villas/timing.h>

namespace villas {
namespace node {

class StatsHook : public Hook {

protected:
	struct stats stats;

	enum stats_format format;
	int verbose;
	int warmup;
	int buckets;

	AFILE *output;
	char *uri;

	sample *last;

public:

	StatsHook(struct path *p, struct node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		format(STATS_FORMAT_HUMAN),
		verbose(0),
		warmup(500),
		buckets(20),
		output(nullptr),
		uri(nullptr)
	{
		stats.state = STATE_DESTROYED;

		/* Register statistic object to path.
		*
		* This allows the path code to update statistics. */
		if (node)
			node->stats = &stats;
	}

	~StatsHook()
	{
		if (uri)
			free(uri);

		if (stats.state != STATE_DESTROYED)
			stats_destroy(&stats);
	}

	virtual void prepare()
	{
		int ret;
		assert(state == STATE_CHECKED);

		ret = stats_init(&stats, buckets, warmup);
		if (ret)
			throw RuntimeError("Failed to initialze stats");

		state = STATE_PREPARED;
	}

	virtual void start()
	{
		assert(state == STATE_PREPARED);

		if (uri) {
			output = afopen(uri, "w+");
			if (!output)
				throw RuntimeError("Failed to open file '{}' for writing", uri);
		}

		state = STATE_STARTED;
	}

	virtual void stop()
	{
		assert(state == STATE_STARTED);

		stats_print(&stats, uri ? output->file : stdout, format, verbose);

		if (uri)
			afclose(output);

		state = STATE_STOPPED;
	}

	virtual void restart()
	{
		assert(state == STATE_STARTED);

		stats_reset(&stats);
	}

	virtual void periodic()
	{
		assert(state == STATE_STARTED);

		stats_print_periodic(&stats, uri ? output->file : stdout, format, verbose, node);
	}

	virtual void parse(json_t *cfg)
	{
		int ret, fmt;
		json_error_t err;

		assert(state != STATE_STARTED);

		const char *f = nullptr;
		const char *u = nullptr;

		ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: b, s?: i, s?: i, s?: s }",
			"format", &f,
			"verbose", &verbose,
			"warmup", &warmup,
			"buckets", &buckets,
			"output", &u
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-stats");

		if (f) {
			fmt = stats_lookup_format(f);
			if (fmt < 0)
				throw ConfigError(cfg, "node-config-hook-stats", "Invalid statistic output format: {}", f);

			format = static_cast<stats_format>(fmt);
		}

		if (u)
			uri = strdup(u);

		state = STATE_PARSED;
	}

	virtual int process(sample *smp)
	{
		struct stats *s = &stats;

		if (last) {
			if (smp->flags & last->flags & SAMPLE_HAS_TS_RECEIVED)
				stats_update(s, STATS_METRIC_GAP_RECEIVED, time_delta(&last->ts.received, &smp->ts.received));

			if (smp->flags & last->flags & SAMPLE_HAS_TS_ORIGIN)
				stats_update(s, STATS_METRIC_GAP_SAMPLE, time_delta(&last->ts.origin, &smp->ts.origin));

			if ((smp->flags & SAMPLE_HAS_TS_ORIGIN) && (smp->flags & SAMPLE_HAS_TS_RECEIVED))
				stats_update(s, STATS_METRIC_OWD, time_delta(&smp->ts.origin, &smp->ts.received));

			if (smp->flags & last->flags & SAMPLE_HAS_SEQUENCE) {
				int dist = smp->sequence - (int32_t) last->sequence;
				if (dist != 1)
					stats_update(s, STATS_METRIC_SMPS_REORDERED, dist);
			}
		}

		sample_incref(smp);

		if (last)
			sample_decref(last);

		last = smp;

		return HOOK_OK;
	}
};

/* Register hook */
static HookPlugin<StatsHook> p(
	"stats",
	"Collect statistics for the current path",
	HOOK_NODE_READ,
	99
);

} /* namespace node */
} /* namespace villas */

/** @} */

