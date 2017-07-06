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
#include <inttypes.h>

#include "node.h"
#include "plugin.h"
#include "nodes/signal.h"

int signal_parse(struct node *n, config_setting_t *cfg)
{
	struct signal *s = n->_vd;

	const char *type;
	
	if (!config_setting_lookup_string(cfg, "signal", &type))
		s->type = TYPE_MIXED;
	else {
		if      (!strcmp(type, "random"))
			s->type = TYPE_RANDOM;
		else if (!strcmp(type, "sine"))
			s->type = TYPE_SINE;
		else if (!strcmp(type, "square"))
			s->type = TYPE_SQUARE;
		else if (!strcmp(type, "triangle"))
			s->type = TYPE_TRIANGLE;
		else if (!strcmp(type, "ramp"))
			s->type = TYPE_RAMP;
		else if (!strcmp(type, "mixed"))
			s->type = TYPE_MIXED;
		else
			cerror(cfg, "Invalid signal type: %s", type);
	}
	
	if (!config_setting_lookup_int(cfg, "limit", &s->limit))
		s->limit = -1;
	
	if (!config_setting_lookup_int(cfg, "values", &s->values))
		s->values = 1;
	
	if (!config_setting_lookup_float(cfg, "rate", &s->rate))
		s->rate = 10;
	
	if (!config_setting_lookup_float(cfg, "frequency", &s->frequency))
		s->frequency = 1;
	
	if (!config_setting_lookup_float(cfg, "amplitude", &s->amplitude))
		s->amplitude = 1;
	
	if (!config_setting_lookup_float(cfg, "stddev", &s->stddev))
		s->stddev = 0.02;
	

	return 0;
}

int signal_open(struct node *n)
{
	struct signal *s = n->_vd;
	
	s->counter = 0;
	s->started = time_now();
	
	s->tfd = timerfd_create_rate(s->rate);

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
	
	assert(cnt == 1);
	
	uint64_t steps = timerfd_wait(s->tfd);
	if (steps > 1)
		warn("Missed steps: %" PRIu64, steps);
	
	struct timespec now = time_now();
	
	double running = time_delta(&s->started, &now);

	smps[0]->ts.origin = 
	smps[0]->ts.received = now;
	smps[0]->sequence = s->counter;
	smps[0]->length = s->values;

	for (int i = 0; i < MIN(s->values, smps[0]->capacity); i++) {
		int rtype = (s->type != TYPE_MIXED) ? s->type : i % 4;
		switch (rtype) {
			case TYPE_RANDOM:   smps[0]->data[i].f += box_muller(0, s->stddev);                                              break;
			case TYPE_SINE:	    smps[0]->data[i].f = s->amplitude *        sin(running * s->frequency * 2 * M_PI);           break;
			case TYPE_TRIANGLE: smps[0]->data[i].f = s->amplitude * (fabs(fmod(running * s->frequency, 1) - .5) - 0.25) * 4; break;
			case TYPE_SQUARE:   smps[0]->data[i].f = s->amplitude * (    (fmod(running * s->frequency, 1) < .5) ? -1 : 1);   break;
			case TYPE_RAMP:     smps[0]->data[i].f = fmod(s->counter, s->rate / s->frequency); /** @todo send as integer? */ break;
		}
	}
	
	s->counter++;
	if (s->limit > 0 && s->counter >= s->limit) {
		info("Reached limit of node %s", node_name(n));
		killme(SIGTERM);
	}

	return 1;
}

char * signal_print(struct node *n)
{
	struct signal *s = n->_vd;
	char *type, *buf = NULL;
	
	switch (s->type) {
		case TYPE_MIXED:    type = "mixed";    break;
		case TYPE_RAMP:     type = "ramp";     break;
		case TYPE_TRIANGLE: type = "triangle"; break;
		case TYPE_SQUARE:   type = "square";   break;
		case TYPE_SINE:     type = "sine";     break;
		case TYPE_RANDOM:   type = "random";   break;
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
		.size = sizeof(struct signal),
		.parse = signal_parse,
		.print = signal_print,
		.start = signal_open,
		.stop = signal_close,
		.read = signal_read,
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)