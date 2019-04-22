/** Node-type for signal generation.
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

#include <math.h>
#include <string.h>

#include <villas/node.h>
#include <villas/plugin.h>
#include <villas/nodes/signal_generator.hpp>

static enum signal_generator::type signal_generator_lookup_type(const char *type)
{
	if      (!strcmp(type, "random"))
		return signal_generator::type::RANDOM;
	else if (!strcmp(type, "sine"))
		return signal_generator::type::SINE;
	else if (!strcmp(type, "square"))
		return signal_generator::type::SQUARE;
	else if (!strcmp(type, "triangle"))
		return signal_generator::type::TRIANGLE;
	else if (!strcmp(type, "ramp"))
		return signal_generator::type::RAMP;
	else if (!strcmp(type, "counter"))
		return signal_generator::type::COUNTER;
	else if (!strcmp(type, "constant"))
		return signal_generator::type::CONSTANT;
	else if (!strcmp(type, "mixed"))
		return signal_generator::type::MIXED;
	else
		return signal_generator::type::INVALID;
}

static const char * signal_generator_type_str(enum signal_generator::type type)
{
	switch (type) {
		case signal_generator::type::CONSTANT:
			return "constant";

		case signal_generator::type::SINE:
			return "sine";

		case signal_generator::type::TRIANGLE:
			return "triangle";

		case signal_generator::type::SQUARE:
			return "square";

		case signal_generator::type::RAMP:
			return "ramp";

		case signal_generator::type::COUNTER:
			return "counter";

		case signal_generator::type::RANDOM:
			return "random";

		case signal_generator::type::MIXED:
			return "mixed";

		default:
			return nullptr;
	}
}

int signal_generator_prepare(struct node *n)
{
	struct signal_generator *s = (struct signal_generator *) n->_vd;

	assert(vlist_length(&n->in.signals) == 0);

	for (unsigned i = 0; i < s->values; i++) {
		struct signal *sig = (struct signal *) alloc(sizeof(struct signal));

		int rtype = s->type == signal_generator::type::MIXED ? i % 7 : s->type;

		sig->name = strdup(signal_generator_type_str((enum signal_generator::type) rtype));
		sig->type = SIGNAL_TYPE_FLOAT; /* All generated signals are of type float */

		vlist_push(&n->in.signals, sig);
	}

	return 0;
}

int signal_generator_parse(struct node *n, json_t *cfg)
{
	struct signal_generator *s = (struct signal_generator *) n->_vd;

	int ret;
	const char *type = nullptr;

	json_error_t err;

	s->rt = 1;
	s->limit = -1;
	s->values = 1;
	s->rate = 10;
	s->frequency = 1;
	s->amplitude = 1;
	s->stddev = 0.2;
	s->offset = 0;
	s->monitor_missed = 1;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: b, s?: i, s?: i, s?: F, s?: F, s?: F, s?: F, s?: F, s?: b}",
		"signal", &type,
		"realtime", &s->rt,
		"limit", &s->limit,
		"values", &s->values,
		"rate", &s->rate,
		"frequency", &s->frequency,
		"amplitude", &s->amplitude,
		"stddev", &s->stddev,
		"offset", &s->offset,
		"monitor_missed", &s->monitor_missed
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (type) {
		ret = signal_generator_lookup_type(type);
		if (ret == -1)
			error("Unknown signal type '%s' of node %s", type, node_name(n));

		s->type = (enum signal_generator::type) ret;
	}
	else
		s->type = signal_generator::type::MIXED;

	return 0;
}

int signal_generator_start(struct node *n)
{
	int ret;
	struct signal_generator *s = (struct signal_generator *) n->_vd;

	s->missed_steps = 0;
	s->counter = 0;
	s->started = time_now();
	s->last = (double *) alloc(sizeof(double) * s->values);

	for (unsigned i = 0; i < s->values; i++)
		s->last[i] = s->offset;

	/* Setup task */
	if (s->rt) {
		ret = task_init(&s->task, s->rate, CLOCK_MONOTONIC);
		if (ret)
			return ret;
	}

	return 0;
}

int signal_generator_stop(struct node *n)
{
	int ret;
	struct signal_generator *s = (struct signal_generator *) n->_vd;

	if (s->rt) {
		ret = task_destroy(&s->task);
		if (ret)
			return ret;
	}

	if (s->missed_steps > 0 && s->monitor_missed)
		warning("Node %s missed a total of %d steps.", node_name(n), s->missed_steps);

	free(s->last);

	return 0;
}

int signal_generator_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct signal_generator *s = (struct signal_generator *) n->_vd;
	struct sample *t = smps[0];

	struct timespec ts;
	int steps;

	assert(cnt == 1);

	/* Throttle output if desired */
	if (s->rt) {
		/* Block until 1/p->rate seconds elapsed */
		steps = task_wait(&s->task);
		if (steps > 1 && s->monitor_missed) {
			debug(5, "Missed steps: %u", steps-1);
			s->missed_steps += steps-1;
		}

		ts = time_now();
	}
	else {
		struct timespec offset = time_from_double(s->counter * 1.0 / s->rate);

		ts = time_add(&s->started, &offset);

		steps = 1;
	}

	double running = time_delta(&s->started, &ts);

	t->flags = SAMPLE_HAS_TS_ORIGIN | SAMPLE_HAS_DATA | SAMPLE_HAS_SEQUENCE;
	t->ts.origin = ts;
	t->sequence = s->counter;
	t->length = MIN(s->values, t->capacity);
	t->signals = &n->in.signals;

	for (unsigned i = 0; i < MIN(s->values, t->capacity); i++) {
		int rtype = (s->type != signal_generator::type::MIXED) ? s->type : i % 7;

		switch (rtype) {
			case signal_generator::type::CONSTANT:
				t->data[i].f = s->offset + s->amplitude;
				break;

			case signal_generator::type::SINE:
				t->data[i].f = s->offset + s->amplitude *        sin(running * s->frequency * 2 * M_PI);
				break;

			case signal_generator::type::TRIANGLE:
				t->data[i].f = s->offset + s->amplitude * (fabs(fmod(running * s->frequency, 1) - .5) - 0.25) * 4;
				break;

			case signal_generator::type::SQUARE:
				t->data[i].f = s->offset + s->amplitude * (    (fmod(running * s->frequency, 1) < .5) ? -1 : 1);
				break;

			case signal_generator::type::RAMP:
				t->data[i].f = s->offset + s->amplitude *       fmod(running, s->frequency);
				break;

			case signal_generator::type::COUNTER:
				t->data[i].f = s->offset + s->amplitude * s->counter;
				break;

			case signal_generator::type::RANDOM:
				s->last[i] += box_muller(0, s->stddev);
				t->data[i].f = s->last[i];
				break;
		}
	}

	if (s->limit > 0 && s->counter >= (unsigned) s->limit) {
		info("Reached limit.");

		n->state = STATE_STOPPING;

		return -1;
	}

	s->counter += steps;

	return 1;
}

char * signal_generator_print(struct node *n)
{
	struct signal_generator *s = (struct signal_generator *) n->_vd;
	char *buf = nullptr;
	const char *type = signal_generator_type_str(s->type);

	strcatf(&buf, "signal=%s, rt=%s, rate=%.2f, values=%d, frequency=%.2f, amplitude=%.2f, stddev=%.2f, offset=%.2f",
		type, s->rt ? "yes" : "no", s->rate, s->values, s->frequency, s->amplitude, s->stddev, s->offset);

	if (s->limit > 0)
		strcatf(&buf, ", limit=%d", s->limit);

	return buf;
}

int signal_generator_poll_fds(struct node *n, int fds[])
{
	struct signal_generator *s = (struct signal_generator *) n->_vd;

	fds[0] = task_fd(&s->task);

	return 1;
}

static struct plugin p = {
	.name = "signal",
	.description = "Signal generator",
	.type = PLUGIN_TYPE_NODE,
	.node = {
		.vectorize	= 1,
		.flags		= NODE_TYPE_PROVIDES_SIGNALS,
		.size		= sizeof(struct signal_generator),
		.parse		= signal_generator_parse,
		.prepare	= signal_generator_prepare,
		.print		= signal_generator_print,
		.start		= signal_generator_start,
		.stop		= signal_generator_stop,
		.read		= signal_generator_read,
		.poll_fds	= signal_generator_poll_fds
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
