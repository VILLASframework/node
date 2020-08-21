/** Node-type for signal generation.
 *
 * @file
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

#include <list>
#include <cmath>
#include <cstring>

#include <villas/node.h>
#include <villas/plugin.h>
#include <villas/exceptions.hpp>
#include <villas/nodes/signal_generator.hpp>

using namespace villas;
using namespace villas::utils;

static enum signal_generator::SignalType signal_generator_lookup_type(const char *type)
{
	if      (!strcmp(type, "random"))
		return signal_generator::SignalType::RANDOM;
	else if (!strcmp(type, "sine"))
		return signal_generator::SignalType::SINE;
	else if (!strcmp(type, "square"))
		return signal_generator::SignalType::SQUARE;
	else if (!strcmp(type, "triangle"))
		return signal_generator::SignalType::TRIANGLE;
	else if (!strcmp(type, "ramp"))
		return signal_generator::SignalType::RAMP;
	else if (!strcmp(type, "counter"))
		return signal_generator::SignalType::COUNTER;
	else if (!strcmp(type, "constant"))
		return signal_generator::SignalType::CONSTANT;
	else if (!strcmp(type, "mixed"))
		return signal_generator::SignalType::MIXED;
	else if (!strcmp(type, "pulse"))
		return signal_generator::SignalType::PULSE;

	throw std::invalid_argument("Invalid signal type");
}

static const char * signal_generator_type_str(enum signal_generator::SignalType type)
{
	switch (type) {
		case signal_generator::SignalType::CONSTANT:
			return "constant";

		case signal_generator::SignalType::SINE:
			return "sine";

		case signal_generator::SignalType::TRIANGLE:
			return "triangle";

		case signal_generator::SignalType::SQUARE:
			return "square";

		case signal_generator::SignalType::RAMP:
			return "ramp";
		case signal_generator::SignalType::COUNTER:
			return "counter";

		case signal_generator::SignalType::RANDOM:
			return "random";

		case signal_generator::SignalType::MIXED:
			return "mixed";
		case signal_generator::SignalType::PULSE:
			return "pulse";
		default:
			return nullptr;
	}
}

int signal_generator_init(struct node *n)
{
	struct signal_generator *s = (struct signal_generator *) n->_vd;

	new (&s->task) Task(CLOCK_MONOTONIC);

	s->rt = 1;
	s->limit = -1;
	s->values = 1;
	s->rate = 10;
	s->monitor_missed = 1;

	s->frequency = nullptr;
	s->amplitude = nullptr;
	s->stddev = nullptr;
	s->offset = nullptr;

	return 0;
}

int signal_generator_destroy(struct node *n)
{
	struct signal_generator *s = (struct signal_generator *) n->_vd;

	s->task.~Task();

	if (s->frequency)
		delete s->frequency;

	if (s->amplitude)
		delete s->amplitude;

	if (s->stddev)
		delete s->stddev;

	if (s->offset)
		delete s->offset;

	return 0;
}

int signal_generator_prepare(struct node *n)
{
	struct signal_generator *s = (struct signal_generator *) n->_vd;

	assert(vlist_length(&n->in.signals) == 0);

	for (unsigned i = 0; i < s->values; i++) {
		int rtype = s->type == signal_generator::SignalType::MIXED ? i % 7 : (int) s->type;
		auto name = signal_generator_type_str((enum signal_generator::SignalType) rtype);
		auto *sig = signal_create(name, nullptr, SignalType::FLOAT);

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

	json_t *json_amplitude = nullptr;
	json_t *json_offset = nullptr;
	json_t *json_frequency = nullptr;
	json_t *json_stddev = nullptr;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: b, s?: i, s?: i, s?: F, s?: o, s?: o, s?: o, s?: o, s?: b }",
		"signal", &type,
		"realtime", &s->rt,
		"limit", &s->limit,
		"values", &s->values,
		"rate", &s->rate,
		"frequency", &json_frequency,
		"amplitude", &json_amplitude,
		"stddev", &json_stddev,
		"offset", &json_offset,
		"monitor_missed", &s->monitor_missed
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	struct desc {
		json_t *json;
		double **array;
		double def_value;
		const char *name;
	};

	std::list<struct desc> arrays = {
		{ json_frequency, &s->frequency, 1, "frequency" },
		{ json_amplitude, &s->amplitude, 1, "amplitude" },
		{ json_stddev, &s->stddev, 0.2, "stddev" },
		{ json_offset, &s->offset, 0, "offset" }
	};

	for (auto &a : arrays) {
		if (*a.array)
			delete *a.array;

		*a.array = new double[s->values];

		if (a.json) {
			switch (json_typeof(a.json)) {
				case JSON_ARRAY:
					if (json_array_size(a.json) != s->values)
						throw ConfigError(a.json, "node-config-node-signal", "Length of values must match");

					size_t i;
					json_t *json_value;
					json_array_foreach(a.json, i, json_value) {
						if (!json_is_real(json_value))
							throw ConfigError(json_value, "node-config-node-signal", "Values must gives as array of float values!");

						(*a.array)[i] = json_real_value(json_value);
					}

					break;

				case JSON_REAL:
					if (!json_is_real(a.json))
						throw ConfigError(a.json, "node-config-node-signal", "Values must gives as array of float values!");

					for (size_t i = 0; i < s->values; i++)
						(*a.array)[i] = json_real_value(a.json);

					break;

				default:
					throw ConfigError(a.json, "node-config-node-signal", "Values must given as array or scalar float value!");
			}
		}
		else {
			for (size_t i = 0; i < s->values; i++)
				(*a.array)[i] = a.def_value;
		}
	}

	if (type)
		s->type = signal_generator_lookup_type(type);
	else
		s->type = signal_generator::SignalType::MIXED;

	return 0;
}

int signal_generator_start(struct node *n)
{
	struct signal_generator *s = (struct signal_generator *) n->_vd;

	s->missed_steps = 0;
	s->counter = 0;
	s->started = time_now();
	s->last = new double[s->values];
	if (!s->last)
		throw MemoryAllocationError();

	for (unsigned i = 0; i < s->values; i++)
		s->last[i] = s->offset[i];

	/* Setup task */
	if (s->rt)
		s->task.setRate(s->rate);

	return 0;
}

int signal_generator_stop(struct node *n)
{
	struct signal_generator *s = (struct signal_generator *) n->_vd;

	if (s->rt)
		s->task.stop();

	if (s->missed_steps > 0 && s->monitor_missed)
		warning("Node %s missed a total of %d steps.", node_name(n), s->missed_steps);

	delete[] s->last;

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
		steps = s->task.wait();
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

	t->flags = (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_DATA | (int) SampleFlags::HAS_SEQUENCE;
	t->ts.origin = ts;
	t->sequence = s->counter;
	t->length = MIN(s->values, t->capacity);
	t->signals = &n->in.signals;

	for (unsigned i = 0; i < MIN(s->values, t->capacity); i++) {
		auto rtype = (s->type != signal_generator::SignalType::MIXED) ? s->type : (signal_generator::SignalType) (i % 7);

		switch (rtype) {
			case signal_generator::SignalType::CONSTANT:
				t->data[i].f = s->offset[i] + s->amplitude[i];
				break;

			case signal_generator::SignalType::SINE:
				t->data[i].f = s->offset[i] + s->amplitude[i] *        sin(running * s->frequency[i] * 2 * M_PI);
				break;

			case signal_generator::SignalType::TRIANGLE:
				t->data[i].f = s->offset[i] + s->amplitude[i] * (fabs(fmod(running * s->frequency[i], 1) - .5) - 0.25) * 4;
				break;

			case signal_generator::SignalType::SQUARE:
				t->data[i].f = s->offset[i] + s->amplitude[i] * (    (fmod(running * s->frequency[i], 1) < .5) ? -1 : 1);
				break;

			case signal_generator::SignalType::RAMP:
				t->data[i].f = s->offset[i] + s->amplitude[i] *       fmod(running, s->frequency[i]);
				break;

			case signal_generator::SignalType::COUNTER:
				t->data[i].f = s->offset[i] + s->amplitude[i] * s->counter;
				break;

			case signal_generator::SignalType::RANDOM:
				s->last[i] += box_muller(0, s->stddev[i]);
				t->data[i].f = s->last[i];
				break;

			case signal_generator::SignalType::MIXED:
				break;
			case signal_generator::SignalType::PULSE:
				t->data[i].f = 5;
				break;
		}
	}

	if (s->limit > 0 && s->counter >= (unsigned) s->limit) {
		info("Reached limit.");

		n->state = State::STOPPING;

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

	strcatf(&buf, "signal=%s, rt=%s, rate=%.2f, values=%d",
		type, s->rt ? "yes" : "no", s->rate, s->values);

	if (s->limit > 0)
		strcatf(&buf, ", limit=%d", s->limit);

	return buf;
}

int signal_generator_poll_fds(struct node *n, int fds[])
{
	struct signal_generator *s = (struct signal_generator *) n->_vd;

	fds[0] = s->task.getFD();

	return 1;
}

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "signal";
	p.description		= "Signal generator";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 1;
	p.node.flags		= (int) NodeFlags::PROVIDES_SIGNALS;
	p.node.size		= sizeof(struct signal_generator);
	p.node.init		= signal_generator_init;
	p.node.destroy		= signal_generator_destroy;
	p.node.parse		= signal_generator_parse;
	p.node.prepare		= signal_generator_prepare;
	p.node.print		= signal_generator_print;
	p.node.start		= signal_generator_start;
	p.node.stop		= signal_generator_stop;
	p.node.read		= signal_generator_read;
	p.node.poll_fds		= signal_generator_poll_fds;

	vlist_init(&p.node.instances);
	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
