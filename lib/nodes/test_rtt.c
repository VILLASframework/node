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

static int test_rtt_case_start(struct test_rtt *t)
{
	int ret;
	char fn[512];

	/* Open file */
	snprintf(fn, sizeof(fn), "%s/test_rtt_%d_%.0f.log", t->output, c->values, c->rate);
	ret = io_init(&t->io, NULL, IO_FORMAT_ALL & ~IO_FORMAT_VALUES);
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
	io_close(&t->io);

	/* Stop timer. */
	task_destroy(&c->task);

	return 0;
}

int test_rtt_parse(struct node *n, config_setting_t *cfg)
{
	struct test_rtt *t = n->_vd;

	int limit, ret;
	int numrates = 0, numvalues = 0;
	int *rates, *values;
	const char *format;

	config_setting_t *cfg_rates, *cfg_values, *cfg_elem;

	if (!config_setting_lookup_int(cfg, "limit", &limit))
		limit = 1000;

	if (!config_setting_lookup_string(cfg, "output", &t->output))
		t->output = ".";

	if (!config_setting_lookup_string(cfg, "format", &format))
		format = "villas";

	if (!config_setting_lookup_float(cfg, "cooldown", &t->cooldown))
		t->cooldown = 1.0;

	cfg_rates = config_setting_get_member(cfg, "rates");
	if (cfg_rates) {
		if (!config_setting_is_array(cfg_rates) || !config_setting_length(cfg_rates))
			cerror(cfg_rates, "The 'rates' setting must be an array of integers with at least one element.");

		numrates = config_setting_length(cfg_rates);
		rates = alloc(sizeof(rates[0]) * numrates);

		for (int i = 0; i < numrates; i++) {
			cfg_elem = config_setting_get_elem(cfg_rates, i);

			if (config_setting_type(cfg_elem) != CONFIG_TYPE_INT)
				cerror(cfg_elem, "The 'rates' setting must be an array of strings");

			rates[i] = config_setting_get_int(cfg_elem);
		}
	}
	else
		cerror(cfg, "Node %s requires setting 'rates' which must be a list of integers", node_name(n));

	cfg_values = config_setting_get_member(cfg, "values");
	if (cfg_values) {
		if (!config_setting_is_array(cfg_values) || !config_setting_length(cfg_values))
			cerror(cfg_values, "The 'values' setting must be an array of integers with at least one element.");

		numvalues = config_setting_length(cfg_values);
		values = alloc(sizeof(values[0]) * numvalues);

		for (int i = 0; i < numvalues; i++) {
			cfg_elem = config_setting_get_elem(cfg_values, i);

			if (config_setting_type(cfg_elem) != CONFIG_TYPE_INT)
				cerror(cfg_elem, "The 'values' setting must be an array of strings");

			values[i] = config_setting_get_int(cfg_elem);
		}
	}
	else
		cerror(cfg, "Node %s requires setting 'values' which must be a list of integers", node_name(n));

	/* Initialize IO module */
	struct plugin *p;

	p = plugin_lookup(PLUGIN_TYPE_FORMAT, format);
	if (!p)
		cerror(cfg, "Invalid value for setting 'format'");

	ret = io_init(&t->io, &p->io, IO_FORMAT_ALL & ~IO_FORMAT_VALUES);
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
		.write		= test_rtt_write
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
