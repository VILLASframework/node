
/** Node type: Node-type for testing Round-trip Time.
 *
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

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <linux/limits.h>

#include <villas/format.hpp>
#include <villas/sample.hpp>
#include <villas/node_compat.hpp>
#include <villas/utils.hpp>
#include <villas/timing.hpp>
#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/test_rtt.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static NodeCompatType p;

static
int test_rtt_case_start(NodeCompat *n, int id)
{
	auto *t = n->getData<struct test_rtt>();
	struct test_rtt_case *c = (struct test_rtt_case *) list_at(&t->cases, id);

	n->logger->info("Starting case #{}: filename={}, rate={}, values={}, limit={}", t->current, c->filename_formatted, c->rate, c->values, c->limit);

	/* Open file */
	t->stream = fopen(c->filename_formatted, "a+");
	if (!t->stream)
		return -1;

	/* Start timer. */
	t->task.setRate(c->rate);

	t->counter = 0;
	t->current = id;

	return 0;
}

static
int test_rtt_case_stop(NodeCompat *n, int id)
{
	int ret;
	auto *t = n->getData<struct test_rtt>();

	/* Stop timer */
	t->task.stop();

	ret = fclose(t->stream);
	if (ret)
		throw SystemError("Failed to close file");

	n->logger->info("Stopping case #{}", id);

	return 0;
}

static
int test_rtt_case_destroy(struct test_rtt_case *c)
{
	if (c->filename)
		free(c->filename);

	if (c->filename_formatted)
		free(c->filename_formatted);

	return 0;
}

int villas::node::test_rtt_prepare(NodeCompat *n)
{
	auto *t = n->getData<struct test_rtt>();

	unsigned max_values = 0;

	/* Take current for time for test case prefix */
	time_t ts = time(nullptr);
	struct tm tm;
	gmtime_r(&ts, &tm);

	for (size_t i = 0; i < list_length(&t->cases); i++) {
		struct test_rtt_case *c = (struct test_rtt_case *) list_at(&t->cases, i);

		if (c->values > max_values)
			max_values = c->values;

		c->filename_formatted = new char[NAME_MAX];
		if (!c->filename_formatted)
			throw MemoryAllocationError();

		strftime(c->filename_formatted, NAME_MAX, c->filename, &tm);
	}

	n->in.signals = std::make_shared<SignalList>(max_values, SignalType::FLOAT);

	return 0;
}

int villas::node::test_rtt_parse(NodeCompat *n, json_t *json)
{
	int ret;
	auto *t = n->getData<struct test_rtt>();

	const char *output = ".";
	const char *prefix = nullptr;

	std::vector<int> rates;
	std::vector<int> values;

	size_t i;
	json_t *json_cases, *json_case, *json_val, *json_format = nullptr;
	json_t *json_rates = nullptr, *json_values = nullptr;
	json_error_t err;

	t->cooldown = 0;

	/* Generate list of test cases */
	ret = list_init(&t->cases);
	if (ret)
		return ret;

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: o, s?: F, s: o }",
		"prefix", &prefix,
		"output", &output,
		"format", &json_format,
		"cooldown", &t->cooldown,
		"cases", &json_cases
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-test-rtt");

	t->output = strdup(output);
	t->prefix = strdup(prefix ? prefix : n->getNameShort().c_str());

	/* Initialize IO module */
	if (!json_format)
		json_format = json_string("villas.binary");

	t->formatter = FormatFactory::make(json_format);
	if (!t->formatter)
		throw ConfigError(json, "node-config-node-test-rtt-format", "Invalid value for setting 'format'");

	/* Construct List of test cases */
	if (!json_is_array(json_cases))
		throw ConfigError(json_cases, "node-config-node-test-rtt-format", "The 'cases' setting must be an array.");

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
			throw ConfigError(json_case, "node-config-node-test-rtt-duration", "The settings 'duration' and 'limit' must be used exclusively");

		if (!json_is_array(json_rates) && !json_is_number(json_rates))
			throw ConfigError(json_case, "node-config-node-test-rtt-rates", "The 'rates' setting must be a real or an array of real numbers");

		if (!json_is_array(json_values) && !json_is_integer(json_values))
			throw ConfigError(json_case, "node-config-node-test-rtt-values", "The 'values' setting must be an integer or an array of integers");

		values.clear();
		rates.clear();

		if (json_is_array(json_rates)) {
			size_t j;
			json_array_foreach(json_rates, j, json_val) {
				if (!json_is_number(json_val))
					throw ConfigError(json_val, "node-config-node-test-rtt-rates", "The 'rates' setting must be an array of real numbers");

				rates.push_back(json_integer_value(json_val));
			}
		}
		else
			rates.push_back(json_number_value(json_rates));

		if (json_is_array(json_values)) {
			size_t j;
			json_array_foreach(json_values, j, json_val) {
				if (!json_is_integer(json_val))
					throw ConfigError(json_val, "node-config-node-test-rtt-values", "The 'values' setting must be an array of integers");

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
				c->node = n;

				c->rate = rate;
				c->values = value;

				if (limit > 0)
					c->limit = limit;
				else if (duration > 0)
					c->limit = duration * c->rate;
				else
					c->limit = 1000; /* default value */

				c->filename = strf("%s/%s_values%d_rate%.0f.log", t->output, t->prefix, c->values, c->rate);

				list_push(&t->cases, c);
			}
		}
	}

	return 0;
}

int villas::node::test_rtt_init(NodeCompat *n)
{
	auto *t = n->getData<struct test_rtt>();

	new (&t->task) Task(CLOCK_MONOTONIC);

	t->formatter = nullptr;

	return 0;
}

int villas::node::test_rtt_destroy(NodeCompat *n)
{
	int ret;
	auto *t = n->getData<struct test_rtt>();

	ret = list_destroy(&t->cases, (dtor_cb_t) test_rtt_case_destroy, true);
	if (ret)
		return ret;

	t->task.~Task();

	if (t->output)
		free(t->output);

	if (t->prefix)
		free(t->prefix);

	if (t->formatter)
		delete t->formatter;

	return 0;
}

char * villas::node::test_rtt_print(NodeCompat *n)
{
	auto *t = n->getData<struct test_rtt>();

	return strf("output=%s, prefix=%s, cooldown=%f, #cases=%zu", t->output, t->prefix, t->cooldown, list_length(&t->cases));
}

int villas::node::test_rtt_start(NodeCompat *n)
{
	int ret;
	struct stat st;
	auto *t = n->getData<struct test_rtt>();
	struct test_rtt_case *c = (struct test_rtt_case *) list_first(&t->cases);

	/* Create folder for results if not present */
	ret = stat(t->output, &st);
	if (ret || !S_ISDIR(st.st_mode)) {
		ret = mkdir(t->output, 0777);
		if (ret)
			throw SystemError("Failed to create output directory: {}", t->output);
	}

	t->formatter->start(n->getInputSignals(false), ~(int) SampleFlags::HAS_DATA);

	t->task.setRate(c->rate);

	t->current = -1;
	t->counter = -1;

	return 0;
}

int villas::node::test_rtt_stop(NodeCompat *n)
{
	int ret;
	auto *t = n->getData<struct test_rtt>();

	if (t->counter >= 0) {
		ret = test_rtt_case_stop(n, t->current);
		if (ret)
			return ret;
	}

	delete t->formatter;

	return 0;
}

int villas::node::test_rtt_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	int ret;
	unsigned i;
	uint64_t steps;

	auto *t = n->getData<struct test_rtt>();

	/* Handle start/stop of new cases */
	if (t->counter == -1) {
		if (t->current < 0) {
			t->current = 0;
		}
		else {
			ret = test_rtt_case_stop(n, t->current);
			if (ret)
				return ret;

			t->current++;
		}

		if ((unsigned) t->current >= list_length(&t->cases)) {
			n->logger->info("This was the last case.");

			n->setState(State::STOPPING);

			return -1;
		}
		else {
			ret = test_rtt_case_start(n, t->current);
			if (ret)
				return ret;
		}
	}

	struct test_rtt_case *c = (struct test_rtt_case *) list_at(&t->cases, t->current);

	/* Wait */
	steps = t->task.wait();
	if (steps > 1)
		n->logger->warn("Skipped {} steps", steps - 1);

	if ((unsigned) t->counter >= c->limit) {
		n->logger->info("Stopping case #{}", t->current);

		t->counter = -1;

		if (t->cooldown) {
			n->logger->info("Entering cooldown phase. Waiting {} seconds...", t->cooldown);
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
			smps[i]->signals = n->getInputSignals(false);

			t->counter++;
		}

		return i;
	}
}

int villas::node::test_rtt_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	auto *t = n->getData<struct test_rtt>();

	if (t->current < 0)
		return 0;

	struct test_rtt_case *c = (struct test_rtt_case *) list_at(&t->cases, t->current);

	unsigned i;
	for (i = 0; i < cnt; i++) {
		if (smps[i]->length != c->values) {
			n->logger->warn("Discarding invalid sample due to mismatching length: expecting={}, has={}", c->values, smps[i]->length);
			continue;
		}

		t->formatter->print(t->stream, smps[i]);
	}

	return i;
}

int villas::node::test_rtt_poll_fds(NodeCompat *n, int fds[])
{
	auto *t = n->getData<struct test_rtt>();

	fds[0] = t->task.getFD();

	return 1;
}

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "test_rtt";
	p.description	= "Test round-trip time with loopback";
	p.vectorize	= 0;
	p.flags		= (int) NodeFactory::Flags::PROVIDES_SIGNALS;
	p.size		= sizeof(struct test_rtt);
	p.parse		= test_rtt_parse;
	p.prepare	= test_rtt_prepare;
	p.init		= test_rtt_init;
	p.destroy	= test_rtt_destroy;
	p.print		= test_rtt_print;
	p.start		= test_rtt_start;
	p.stop		= test_rtt_stop;
	p.read		= test_rtt_read;
	p.write		= test_rtt_write;

	static NodeCompatFactory ncp(&p);
}
