/** Node type: Node-type for testing Round-trip Time.
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

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <linux/limits.h>

#include <villas/node.h>
#include <villas/sample.h>
#include <villas/timing.h>
#include <villas/plugin.h>
#include <villas/nodes/test_rtt.hpp>

using namespace villas;
using namespace villas::utils;

static struct plugin p;

static int test_rtt_case_start(struct test_rtt *t, int id)
{
	int ret;
	struct test_rtt_case *c = (struct test_rtt_case *) vlist_at(&t->cases, id);

	info("Starting case #%d: filename=%s, rate=%f, values=%d, limit=%d", t->current, c->filename_formatted, c->rate, c->values, c->limit);

	/* Open file */
	ret = io_open(&t->io, c->filename_formatted);
	if (ret)
		return ret;

	/* Start timer. */
	t->task.setRate(c->rate);

	t->counter = 0;
	t->current = id;

	return 0;
}

static int test_rtt_case_stop(struct test_rtt *t, int id)
{
	int ret;

	/* Stop timer */
	t->task.stop();

	/* Close file */
	ret = io_close(&t->io);
	if (ret)
		return ret;

	info("Stopping case #%d", id);

	return 0;
}

static int test_rtt_case_destroy(struct test_rtt_case *c)
{
	if (c->filename)
		free(c->filename);

	if (c->filename_formatted)
		free(c->filename_formatted);

	return 0;
}

int test_rtt_prepare(struct vnode *n)
{
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	int ret;
	unsigned max_values = 0;

	/* Take current for time for test case prefix */
	time_t ts = time(nullptr);
	struct tm tm;
	gmtime_r(&ts, &tm);

	for (size_t i = 0; i < vlist_length(&t->cases); i++) {
		struct test_rtt_case *c = (struct test_rtt_case *) vlist_at(&t->cases, i);

		if (c->values > max_values)
			max_values = c->values;

		c->filename_formatted = new char[NAME_MAX];
		if (!c->filename_formatted)
			throw MemoryAllocationError();

		strftime(c->filename_formatted, NAME_MAX, c->filename, &tm);
	}

	ret = signal_list_generate(&n->in.signals, max_values, SignalType::FLOAT);
	if (ret)
		return ret;

	return 0;
}

int test_rtt_parse(struct vnode *n, json_t *cfg)
{
	int ret;
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	const char *format = "villas.binary";
	const char *output = ".";
	const char *prefix = node_name_short(n);

	std::vector<int> rates;
	std::vector<int> values;

	size_t i;
	json_t *json_cases, *json_case, *json_val;
	json_t *json_rates = nullptr, *json_values = nullptr;
	json_error_t err;

	t->cooldown = 0;

	/* Generate list of test cases */
	ret = vlist_init(&t->cases);
	if (ret)
		return ret;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s, s?: s, s?: F, s: o }",
		"prefix", &prefix,
		"output", &output,
		"format", &format,
		"cooldown", &t->cooldown,
		"cases", &json_cases
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	t->output = strdup(output);
	t->prefix = strdup(prefix);

	/* Initialize IO module */
	t->format = format_type_lookup(format);
	if (!t->format)
		error("Invalid value for setting 'format' of node %s", node_name(n));


	/* Construct vlist of test cases */
	if (!json_is_array(json_cases))
		error("The 'cases' setting of node %s must be an array.", node_name(n));

	json_array_foreach(json_cases, i, json_case) {
		int limit = -1;
		double duration = -1; /* in secs */

		ret = json_unpack_ex(json_case, &err, 0, "{ s: o, s: o, s?: i, s?: F }",
			"rates", &json_rates,
			"values", &json_values,
			"limit", &limit,
			"duration", &duration
		);

		if (limit > 0 && duration > 0)
			error("The settings 'duration' and 'limit' of node %s must be used exclusively", node_name(n));

		if (!json_is_array(json_rates) && !json_is_number(json_rates))
			error("The 'rates' setting of node %s must be a real or an array of real numbers", node_name(n));

		if (!json_is_array(json_values) && !json_is_integer(json_values))
			error("The 'values' setting of node %s must be an integer or an array of integers", node_name(n));

		values.clear();
		rates.clear();

		if (json_is_array(json_rates)) {
			size_t j;
			json_array_foreach(json_rates, j, json_val) {
				if (!json_is_number(json_val))
					error("The 'rates' setting of node %s must be an array of real numbers", node_name(n));

				rates.push_back(json_integer_value(json_val));
			}
		}
		else
			rates.push_back(json_number_value(json_rates));

		if (json_is_array(json_values)) {
			size_t j;
			json_array_foreach(json_values, j, json_val) {
				if (!json_is_integer(json_val))
					error("The 'values' setting of node %s must be an array of integers", node_name(n));

				values.push_back(json_integer_value(json_val));
			}
		}
		else
			values.push_back(json_integer_value(json_values));

		for (int rate : rates) {
			for (int value : values) {
				auto *c = new struct test_rtt_case;
				if (!c)
					throw MemoryAllocationError();

				c->filename = nullptr;
				c->filename_formatted = nullptr;

				c->rate = rate;
				c->values = value;

				if (limit > 0)
					c->limit = limit;
				else if (duration > 0)
					c->limit = duration * c->rate;
				else
					c->limit = 1000; /* default value */

				c->filename = strf("%s/%s_values%d_rate%.0f.log", t->output, t->prefix, c->values, c->rate);

				vlist_push(&t->cases, c);
			}
		}
	}

	return 0;
}

int test_rtt_init(struct vnode *n)
{
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	new (&t->task) Task(CLOCK_MONOTONIC);

	return 0;
}

int test_rtt_destroy(struct vnode *n)
{
	int ret;
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	ret = vlist_destroy(&t->cases, (dtor_cb_t) test_rtt_case_destroy, true);
	if (ret)
		return ret;

	t->task.~Task();

	if (t->output)
		free(t->output);

	if (t->prefix)
		free(t->prefix);

	return 0;
}

char * test_rtt_print(struct vnode *n)
{
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	return strf("output=%s, prefix=%s, cooldown=%f, #cases=%zu", t->output, t->prefix, t->cooldown, vlist_length(&t->cases));
}

int test_rtt_start(struct vnode *n)
{
	int ret;
	struct stat st;
	struct test_rtt *t = (struct test_rtt *) n->_vd;
	struct test_rtt_case *c = (struct test_rtt_case *) vlist_first(&t->cases);

	/* Create folder for results if not present */
	ret = stat(t->output, &st);
	if (ret || !S_ISDIR(st.st_mode)) {
		ret = mkdir(t->output, 0777);
		if (ret) {
			warning("Failed to create output directory: %s", t->output);
			return ret;
		}
	}

	ret = io_init(&t->io, t->format, &n->in.signals, (int) SampleFlags::HAS_ALL & ~(int) SampleFlags::HAS_DATA);
	if (ret)
		return ret;

	ret = io_check(&t->io);
	if (ret)
		return ret;

	t->task.setRate(c->rate);

	t->current = -1;
	t->counter = -1;

	return 0;
}

int test_rtt_stop(struct vnode *n)
{
	int ret;
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	if (t->counter >= 0) {
		ret = test_rtt_case_stop(t, t->current);
		if (ret)
			return ret;
	}

	ret = io_destroy(&t->io);
	if (ret)
		return ret;

	return 0;
}

int test_rtt_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int ret;
	unsigned i;
	uint64_t steps;

	struct test_rtt *t = (struct test_rtt *) n->_vd;

	/* Handle start/stop of new cases */
	if (t->counter == -1) {
		if (t->current < 0) {
			t->current = 0;
		}
		else {
			ret = test_rtt_case_stop(t, t->current);
			if (ret)
				return ret;

			t->current++;
		}

		if ((unsigned) t->current >= vlist_length(&t->cases)) {
			info("This was the last case. Stopping node %s", node_name(n));

			n->state = State::STOPPING;

			return -1;
		}
		else {
			ret = test_rtt_case_start(t, t->current);
			if (ret)
				return ret;
		}
	}

	struct test_rtt_case *c = (struct test_rtt_case *) vlist_at(&t->cases, t->current);

	/* Wait */
	steps = t->task.wait();
	if (steps > 1)
		warning("Skipped %ld steps", (long) (steps - 1));

	if ((unsigned) t->counter >= c->limit) {
		info("Stopping case #%d", t->current);

		t->counter = -1;

		if (t->cooldown) {
			info("Entering cooldown phase. Waiting %f seconds...", t->cooldown);
			t->task.setTimeout(t->cooldown);
		}

		return 0;
	}
	else {
		struct timespec now = time_now();

		/* Prepare samples */
		for (i = 0; i < cnt; i++) {
			smps[i]->length = c->values;
			smps[i]->sequence = t->counter;
			smps[i]->ts.origin = now;
			smps[i]->flags = (int) SampleFlags::HAS_DATA | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_TS_ORIGIN;
			smps[i]->signals = &n->in.signals;

			t->counter++;
		}

		return i;
	}
}

int test_rtt_write(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	if (t->current < 0)
		return 0;

	struct test_rtt_case *c = (struct test_rtt_case *) vlist_at(&t->cases, t->current);

	unsigned i;
	for (i = 0; i < cnt; i++) {
		if (smps[i]->length != c->values) {
			warning("Discarding invalid sample due to mismatching length: expecting=%d, has=%d", c->values, smps[i]->length);
			continue;
		}

		io_print(&t->io, &smps[i], 1);
	}

	return i;
}

int test_rtt_poll_fds(struct vnode *n, int fds[])
{
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	fds[0] = t->task.getFD();

	return 1;
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "test_rtt";
	p.description		= "Test round-trip time with loopback";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 0;
	p.node.flags		= (int) NodeFlags::PROVIDES_SIGNALS;
	p.node.size		= sizeof(struct test_rtt);
	p.node.parse		= test_rtt_parse;
	p.node.prepare		= test_rtt_prepare;
	p.node.init		= test_rtt_init;
	p.node.destroy		= test_rtt_destroy;
	p.node.print		= test_rtt_print;
	p.node.start		= test_rtt_start;
	p.node.stop		= test_rtt_stop;
	p.node.read		= test_rtt_read;
	p.node.write		= test_rtt_write;

	int ret = vlist_init(&p.node.instances);
	if (!ret)
		vlist_init_and_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
