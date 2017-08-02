/** Node-type for signal generation.
 *
 * @file
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

#include <math.h>

#include "node.h"
#include "plugin.h"
#include "nodes/signal.h"

enum signal_type signal_lookup_type(const char *type)
{
	if      (!strcmp(type, "random"))
		return SIGNAL_TYPE_RANDOM;
	else if (!strcmp(type, "sine"))
		return SIGNAL_TYPE_SINE;
	else if (!strcmp(type, "square"))
		return SIGNAL_TYPE_SQUARE;
	else if (!strcmp(type, "triangle"))
		return SIGNAL_TYPE_TRIANGLE;
	else if (!strcmp(type, "ramp"))
		return SIGNAL_TYPE_RAMP;
	else if (!strcmp(type, "mixed"))
		return SIGNAL_TYPE_MIXED;
	else
		return -1;
}

int signal_parse(struct node *n, json_t *cfg)
{
	struct signal *s = n->_vd;

	int ret;
	const char *type = NULL;

	json_error_t err;

	s->rt = 1;
	s->limit = -1;
	s->values = 1;
	s->rate = 10;
	s->frequency = 1;
	s->amplitude = 1;
	s->stddev = 0.02;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: b, s?: i, s?: i, s?: f, s?: f, s?: f, s?: f }",
		"signal", &type,
		"realtime", &s->rt,
		"limit", &s->limit,
		"values", &s->values,
		"rate", &s->rate,
		"frequency", &s->frequency,
		"amplitude", &s->amplitude,
		"stddev", &s->stddev
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));


	if (type) {
		ret = signal_lookup_type(type);
		if (ret == -1)
			error("Unknown signal type '%s' of node %s", type, node_name(n));

		s->type = ret;
	}
	else
		s->type = SIGNAL_TYPE_MIXED;

	return 0;
}

int signal_parse_cli(struct node *n, int argc, char *argv[])
{
	int ret;
	char *type;

	struct signal *s = n->_vd;

	/* Default values */
	s->rate = 10;
	s->frequency = 1;
	s->amplitude = 1;
	s->stddev = 0.02;
	s->type = SIGNAL_TYPE_MIXED;
	s->rt = 1;
	s->values = 1;
	s->limit = -1;

	/* Parse optional command line arguments */
	char c, *endptr;
	while ((c = getopt(argc, argv, "v:r:f:l:a:D:n")) != -1) {
		switch (c) {
			case 'n':
				s->rt = 0;
				break;
			case 'l':
				s->limit = strtoul(optarg, &endptr, 10);
				goto check;
			case 'v':
				s->values = strtoul(optarg, &endptr, 10);
				goto check;
			case 'r':
				s->rate = strtof(optarg, &endptr);
				goto check;
			case 'f':
				s->frequency = strtof(optarg, &endptr);
				goto check;
			case 'a':
				s->amplitude = strtof(optarg, &endptr);
				goto check;
			case 'D':
				s->stddev = strtof(optarg, &endptr);
				goto check;
			case '?':
				break;
		}

		continue;

check:		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);
	}

	if (argc != optind + 1)
		return -1;

	type = argv[optind];

	ret = signal_lookup_type(type);
	if (ret == -1)
		error("Invalid signal type: %s", type);

	s->type = ret;

	return 0;
}

int signal_open(struct node *n)
{
	struct signal *s = n->_vd;

	s->counter = 0;
	s->started = time_now();

	/* Setup timer */
	if (s->rt) {
		s->tfd = timerfd_create_rate(s->rate);
		if (s->tfd < 0)
			return -1;
	}
	else
		s->tfd = -1;

	return 0;
}

int signal_close(struct node *n)
{
	struct signal* s = n->_vd;

	close(s->tfd);

	return 0;
}

int signal_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct signal *s = n->_vd;
	struct sample *t = smps[0];

	struct timespec now;
	int steps;

	assert(cnt == 1);

	/* Throttle output if desired */
	if (s->rt) {
		/* Block until 1/p->rate seconds elapsed */
		steps = timerfd_wait(s->tfd);
		if (steps > 1)
			warn("Missed steps: %u", steps);

		now = time_now();
	}
	else {
		struct timespec offset = time_from_double(s->counter * 1.0 / s->rate);

		now = time_add(&s->started, &offset);

		steps = 1;
	}

	double running = time_delta(&s->started, &now);

	t->ts.origin =
	t->ts.received = now;
	t->sequence = s->counter;
	t->length = s->values;

	for (int i = 0; i < MIN(s->values, t->capacity); i++) {
		int rtype = (s->type != SIGNAL_TYPE_MIXED) ? s->type : i % 4;
		switch (rtype) {
			case SIGNAL_TYPE_RANDOM:   t->data[i].f += box_muller(0, s->stddev);                                              break;
			case SIGNAL_TYPE_SINE:	   t->data[i].f = s->amplitude *        sin(running * s->frequency * 2 * M_PI);           break;
			case SIGNAL_TYPE_TRIANGLE: t->data[i].f = s->amplitude * (fabs(fmod(running * s->frequency, 1) - .5) - 0.25) * 4; break;
			case SIGNAL_TYPE_SQUARE:   t->data[i].f = s->amplitude * (    (fmod(running * s->frequency, 1) < .5) ? -1 : 1);   break;
			case SIGNAL_TYPE_RAMP:     t->data[i].f = fmod(s->counter, s->rate / s->frequency); /** @todo send as integer? */ break;
		}
	}

	if (s->limit > 0 && s->counter >= s->limit) {
		info("Reached limit of node %s", node_name(n));
		killme(SIGTERM);
		pause();
	}

	s->counter += steps;

	return 1;
}

char * signal_print(struct node *n)
{
	struct signal *s = n->_vd;
	char *type, *buf = NULL;

	switch (s->type) {
		case SIGNAL_TYPE_MIXED:    type = "mixed";    break;
		case SIGNAL_TYPE_RAMP:     type = "ramp";     break;
		case SIGNAL_TYPE_TRIANGLE: type = "triangle"; break;
		case SIGNAL_TYPE_SQUARE:   type = "square";   break;
		case SIGNAL_TYPE_SINE:     type = "sine";     break;
		case SIGNAL_TYPE_RANDOM:   type = "random";   break;
		default: return NULL;
	}

	strcatf(&buf, "signal=%s, rate=%.2f, values=%d, frequency=%.2f, amplitude=%.2f, stddev=%.2f",
		type, s->rate, s->values, s->frequency, s->amplitude, s->stddev);

	if (s->limit > 0)
		strcatf(&buf, ", limit=%d", s->limit);

	return buf;
};

static struct plugin p = {
	.name = "signal",
	.description = "Signal generation",
	.type = PLUGIN_TYPE_NODE,
	.node = {
		.vectorize = 1,
		.size  = sizeof(struct signal),
		.parse = signal_parse,
		.parse_cli = signal_parse_cli,
		.print = signal_print,
		.start = signal_open,
		.stop  = signal_close,
		.read  = signal_read,
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
