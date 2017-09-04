/** Node type: Node-type for testing Round-trip Time.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include "timing.h"
#include "plugin.h"
#include "nodes/test_rtt.h"

static int test_rtt_case_start(struct test_rtt *t, int caseno)
{
	int ret;
	char fn[512];

	struct test_rtt_case *c = list_at(&t->cases, caseno);

	/* Open file */
	snprintf(fn, sizeof(fn), "%s/test_rtt_%d_%.0f.log", t->output, c->values, c->rate);
	ret = io_init(&t->io, NULL, FORMAT_ALL & ~FORMAT_VALUES);
	if (ret)
		return ret;

	/* Start timer. */
	ret = task_init(&t->timer, c->rate);
	if (ret)
		serror("Failed to create timer");

	return 0;
}

static int test_rtt_case_stop(struct test_rtt_case *c)
{
	/* Close file */
	io_close(&c->io);

	/* Stop timer. */
	task_destroy(&c->task);

	return 0;
}

int test_rtt_parse(struct node *n, json_t *cfg)
{
	struct test_rtt *t = n->_vd;
	struct plugin *p;

	int ret, numrates, numvalues, limit = 1000;
	int *rates, *values;

	const char *format = "villas-human";
	const char *output = ".";

	size_t index;
	json_t *cfg_rates, *cfg_values, *cfg_val;
	json_error_t err;

	t->cooldown = 1.0;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: s, s?: s, s: F, s: o, s: o }",
		"limit", &limit,
		"output", &output,
		"format", &format,
		"cooldown", &t->cooldown,
		"rates", &cfg_rates,
		"values", &cfg_values
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	t->output = strdup(output);

	if (cfg_rates) {
		if (!json_is_array(cfg_rates) || json_array_size(cfg_rates) < 1)
			error("The 'rates' setting of node %s must be an array of integers with at least one element.", node_name(n));

		numrates = json_array_size(cfg_rates);
		rates = alloc(sizeof(rates[0]) * numrates);

		json_array_foreach(cfg_rates, index, cfg_val) {
			if (!json_is_integer(cfg_val))
				error("The 'rates' setting of node %s must be an array of integers", node_name(n));

			rates[index] = json_integer_value(cfg_val);
		}
	}

	if (cfg_values) {
		if (!json_is_array(cfg_values) || json_array_size(cfg_values) < 1)
			error("The 'values' setting of node %s must be an array of integers with at least one element.", node_name(n));

		numvalues = json_array_size(cfg_values);
		rates = alloc(sizeof(rates[0]) * numvalues);

		json_array_foreach(cfg_values, index, cfg_val) {
			if (!json_is_integer(cfg_val))
				error("The 'values' setting of node %s must be an array of integers", node_name(n));

			values[index] = json_integer_value(cfg_val);
		}
	}

	/* Initialize IO module */
	p = plugin_lookup(PLUGIN_TYPE_FORMAT, format);
	if (!p)
		error("Invalid value for setting 'format' of node %s", node_name(n));

	ret = io_init(&t->io, &p->io, FORMAT_ALL & ~FORMAT_VALUES);
	if (ret)
		return ret;

	/* Generate list of test cases */
	list_init(&t->cases);

	for (int i = 0; i < numrates; i++) {
		for (int j = 0; j < numvalues; j++) {
			struct test_rtt_case *c = alloc(sizeof(struct test_rtt_case));

			c->rate = rates[i];
			c->values = values[j];
			c->limit = limit;

			list_push(&t->cases, c);
		}
	}

	return 0;
}

char * test_rtt_print(struct node *n)
{
	struct test_rtt *t = n->_vd;

	return strf("output=%s, cooldown=%f, #cases=%zu", t->output, t->cooldown, list_length(&t->cases));
}

int test_rtt_start(struct node *n)
{
	struct test_rtt *t = n->_vd;

	t->current = 0;

	test_rtt_case_start(t);

	return 0;
}

int test_rtt_stop(struct node *n)
{
	int ret;
	struct test_rtt *t = n->_vd;

	ret = test_rtt_case_stop(t->current);
	if (ret)
		error("Failed to stop test case");

	list_destroy(&t->cases, NULL, true);

	return 0;
}

int test_rtt_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret, i;
	uint64_t steps;

	struct test_rtt *t = n->_vd;
	struct test_rtt_case *c = t->current;

	/* Wait */
	steps = task_wait_until_next_period(&c->task);
	if (steps > 1)
		warn("Skipped %zu samples", steps - 1);

	struct timespec now = time_now();

	/* Prepare samples. */
	for (i = 0; i < cnt; i++) {
		if (c->counter >= c->limit) {
			info("Reached limit. Terminating.");
			killme(SIGTERM);
			pause();
		}

		int values = c->values;
		if (smps[i]->capacity < MAX(2, values)) {
			values = smps[i]->capacity;
			warn("Sample capacity too small. Limiting to %d values.", values);
		}

		smps[i]->data[0].i = c->values;
		smps[i]->data[1].f = c->rate;

		smps[i]->length = values;
		smps[i]->sequence = c->counter++;
		smps[i]->ts.origin = now;
	}

	return i;
}

int test_rtt_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct test_rtt *t = n->_vd;

	int i;

	for (i = 0; i < cnt; i++) {
		if (smps[i]->length != c->values) {
			warn("Discarding invalid sample");
			continue;
		}

		io_print(&t->io, smps[i], 1);
	}

	return i;
}

int test_rtt_fd(struct node *n)
{
	struct test_rtt *t = n->_vd;

	return task_fd(&t->task);
}

static struct plugin p = {
	.name		= "test_rtt",
	.description	= "Test round-trip time with loopback",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct test_rtt),
		.parse		= test_rtt_parse,
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
