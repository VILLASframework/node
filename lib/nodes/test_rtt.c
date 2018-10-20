/** Node type: Node-type for testing Round-trip Time.
 *
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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <villas/node.h>
#include <villas/sample.h>
#include <villas/timing.h>
#include <villas/plugin.h>
#include <villas/nodes/test_rtt.h>

static int test_rtt_case_start(struct test_rtt *t, int id)
{
	int ret;
	struct test_rtt_case *c = (struct test_rtt_case *) list_at(&t->cases, id);

	/* Open file */
	ret = io_open(&t->io, c->filename);
	if (ret)
		return ret;

	/* Start timer. */
	ret = task_set_rate(&t->task, c->rate);
	if (ret)
		return ret;

	t->counter = 0;
	t->current = id;

	return 0;
}

static int test_rtt_case_stop(struct test_rtt *t, int id)
{
	int ret;

	/* Stop timer */
	ret = task_set_rate(&t->task, 0);
	if (ret)
		return ret;

	/* Close file */
	ret = io_close(&t->io);
	if (ret)
		return ret;

	return 0;
}

int test_rtt_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	const char *format = "villas.binary";
	const char *output = ".";
	const char *prefix = node_name_short(n);

	int *rates = NULL;
	int *values = NULL;

	int numrates = 0;
	int numvalues = 0;

	size_t i;
	json_t *json_cases, *json_case, *json_val;
	json_t *json_rates = NULL, *json_values = NULL;
	json_error_t err;

	t->cooldown = 1.0;

	/* Generate list of test cases */
	list_init(&t->cases);

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


	/* Construct list of test cases */
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

		if (json_is_array(json_rates))
			numrates = json_array_size(json_rates);
		else if (json_is_number(json_rates))
			numrates = 1;
		else
			error("The 'rates' setting of node %s must be a real or an array of real numbers", node_name(n));

		if (json_is_array(json_values))
			numvalues = json_array_size(json_values);
		else if (json_is_integer(json_values))
			numvalues = 1;
		else
			error("The 'values' setting of node %s must be an integer or an array of integers", node_name(n));

		rates  = realloc(rates, sizeof(rates[0]) * numrates);
		values = realloc(values, sizeof(values[0]) * numvalues);

		if (json_is_array(json_rates)) {
			size_t j;
			json_array_foreach(json_rates, j, json_val) {
				if (!json_is_number(json_val))
					error("The 'rates' setting of node %s must be an array of real numbers", node_name(n));

				rates[i] = json_integer_value(json_val);
			}
		}
		else
			rates[0] = json_number_value(json_rates);

		if (json_is_array(json_values)) {
			size_t j;
			json_array_foreach(json_values, j, json_val) {
				if (!json_is_integer(json_val))
					error("The 'values' setting of node %s must be an array of integers", node_name(n));

				values[j] = json_integer_value(json_val);
				if (values[j] < 2)
					error("Each 'values' entry must be at least 2 or larger");
			}
		}
		else {
			values[0] = json_integer_value(json_values);
			if (values[0] <= 2)
				error("Each 'values' entry must be at least 2 or larger");
		}

		for (int i = 0; i < numrates; i++) {
			for (int j = 0; j < numvalues; j++) {
				struct test_rtt_case *c = (struct test_rtt_case *) alloc(sizeof(struct test_rtt_case));

				c->rate = rates[i];
				c->values = values[j];

				if (limit > 0)
					c->limit = limit;
				else if (duration > 0)
					c->limit = duration * c->rate;
				else
					c->limit = 1000; /* default value */

				c->filename = strf("%s/%s_%d_%.0f.log", t->output, t->prefix, c->values, c->rate);

				list_push(&t->cases, c);
			}
		}
	}

	free(values);
	free(rates);

	return 0;
}

int test_rtt_destroy(struct node *n)
{
	int ret;
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	ret = list_destroy(&t->cases, NULL, true);
	if (ret)
		return ret;

	ret = task_destroy(&t->task);
	if (ret)
		return ret;

	if (t->output)
		free(t->output);

	if (t->prefix)
		free(t->prefix);

	return 0;
}

char * test_rtt_print(struct node *n)
{
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	return strf("output=%s, prefix=%s, cooldown=%f, #cases=%zu", t->output, t->prefix, t->cooldown, list_length(&t->cases));
}

int test_rtt_start(struct node *n)
{
	int ret;
	struct stat st;
	struct test_rtt *t = (struct test_rtt *) n->_vd;
	struct test_rtt_case *c = list_first(&t->cases);

	/* Create folder for results if not present */
	ret = stat(t->output, &st);
	if (ret || !S_ISDIR(st.st_mode)) {
		ret = mkdir(t->output, 0777);
		if (ret)
			return ret;
	}

	ret = io_init(&t->io, t->format, &n->signals, SAMPLE_HAS_ALL & ~SAMPLE_HAS_DATA);
	if (ret)
		return ret;

	ret = io_check(&t->io);
	if (ret)
		return ret;

	ret = io_check(&t->io);
	if (ret)
		return ret;

	ret = task_init(&t->task, c->rate, CLOCK_MONOTONIC);
	if (ret)
		return ret;

	t->current = -1;
	t->counter = -1;

	return 0;
}

int test_rtt_stop(struct node *n)
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

int test_rtt_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int i, ret, values;
	uint64_t steps;

	struct test_rtt *t = (struct test_rtt *) n->_vd;

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

		if (t->current >= list_length(&t->cases)) {
			info("This was the last case. Terminating.");
			killme(SIGTERM);
			pause();
		}
		else {
			struct test_rtt_case *c = (struct test_rtt_case *) list_at(&t->cases, t->current);
			info("Starting case #%d: filename=%s, rate=%f, values=%d, limit=%d", t->current, c->filename, c->rate, c->values, c->limit);
			ret = test_rtt_case_start(t, t->current);
			if (ret)
				return ret;
		}
	}

	struct test_rtt_case *c = (struct test_rtt_case *) list_at(&t->cases, t->current);

	/* Wait */
	steps = task_wait(&t->task);
	if (steps > 1)
		warn("Skipped %ld steps", (long) (steps - 1));

	struct timespec now = time_now();

	/* Prepare samples */
	for (i = 0; i < cnt; i++) {
		values = c->values;
		if (smps[i]->capacity < values) {
			values = smps[i]->capacity;
			warn("Sample capacity too small. Limiting to %d values.", values);
		}

		smps[i]->length = values;
		smps[i]->sequence = t->counter;
		smps[i]->ts.origin = now;
		smps[i]->flags = SAMPLE_HAS_DATA | SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_TS_ORIGIN;

		t->counter++;
	}

	if (t->counter >= c->limit) {
		info("Entering cooldown phase. Waiting %f seconds...", t->cooldown);

		t->counter = -1;

		ret = task_set_timeout(&t->task, t->cooldown);
		if (ret < 0)
			return ret;
	}

	return i;
}

int test_rtt_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	if (t->current < 0)
		return 0;

	struct test_rtt_case *c = (struct test_rtt_case *) list_at(&t->cases, t->current);

	int i;
	for (i = 0; i < cnt; i++) {
		if (smps[i]->length != c->values) {
			warn("Discarding invalid sample due to mismatching length: expecting=%d, has=%d", c->values, smps[i]->length);
			continue;
		}

		io_print(&t->io, &smps[i], 1);
	}

	return i;
}

int test_rtt_fd(struct node *n)
{
	struct test_rtt *t = (struct test_rtt *) n->_vd;

	return task_fd(&t->task);
}

static struct plugin p = {
	.name		= "test_rtt",
	.description	= "Test round-trip time with loopback",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.flags		= NODE_TYPE_PROVIDES_SIGNALS,
		.size		= sizeof(struct test_rtt),
		.parse		= test_rtt_parse,
		.destroy	= test_rtt_destroy,
		.print		= test_rtt_print,
		.start		= test_rtt_start,
		.stop		= test_rtt_stop,
		.read		= test_rtt_read,
		.write		= test_rtt_write,
		.fd		= test_rtt_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
