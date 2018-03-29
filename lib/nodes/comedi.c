/** Node type: comedi
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <string.h>

#include <villas/plugin.h>
#include <villas/nodes/comedi.h>
#include <villas/utils.h>
#include <villas/io_format.h>

static int comedi_parse_direction(struct comedi_direction *d, json_t *cfg)
{
	int ret;

	json_t *json_chans;
	json_error_t err;

	d->subdevice = -1;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s: o, s: F }",
		"subdevice", &d->subdevice,
		"channels", &json_chans,
		"rate", &d->rate
	);
	if (ret)
		jerror(&err, "Failed to parse configuration");

	if (!json_is_array(json_chans))
		return -1;

	size_t i;
	json_t *json_chan;

	json_array_foreach(json_chans, i, json_chan) {
		if (!json_is_integer(json_chan))
			return -1;


	}

	return 0;
}

static int comedi_start_in(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction *d = &c->in;

	if (d->subdevice >= 0) {

	}
	else {
		d->subdevice = comedi_find_subdevice_by_type(c->it, COMEDI_SUBD_AI, 0);
		if (d->subdevice < 0)
			error("Cannot find analog input device for node '%s'", node_name(n));
	}

	/* Check if subdevice is usable */
	ret = comedi_get_subdevice_type(c->it, d->subdevice);
	if (ret != COMEDI_SUBD_AI)
		error("Input subdevice of node '%s' is not an analog input", node_name(n));

	ret = comedi_get_subdevice_flags(c->it, d->subdevice);
	if (ret )

	ret = comedi_lock(c->it, d->subdevice);
	if (ret)
		error("Failed to lock subdevice %d for node '%s'", d->subdevice, node_name(n));

#if 0
	comedi_cmd cmd = {
		.subdev = d->subdevice,
		.flags = CMDF_READ,
		.start_src = TRIG_INT,
		.start_arg = 0,
		.scan_begin_src = TRIG_TIMER,
		.scan_begin_arg = 1e9 / c->in.rate,
		.convert_src = TRIG_NOW,
		.convert_arg = 0,
		.scan_end_src = TRIG_COUNT,
		.scan_end_arg = c->in.chanlist_len,
		.stop_src = TRIG_NONE,
		.stop_arg = 0,
		.chanlist = c->in.chanlist,
		.chanlist_len = c->in.chanlist_len,
	};
#endif

	return 0;
}

static int comedi_start_out(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction *d = &c->out;

	ret = comedi_get_subdevice_type(c->it, d->subdevice);
	if (ret != COMEDI_SUBD_AO)
		error("Output subdevice of node '%s' is not an analog output", node_name(n));

	ret = comedi_get_subdevice_flags(c->it, d->subdevice);
	if (ret )

	ret = comedi_lock(c->it, d->subdevice);
	if (ret)
		error("Failed to lock subdevice %d for node '%s'", d->subdevice, node_name(n));

#if 0
	comedi_cmd cmd = {
		.subdev = c->out.subdevice,
		.flags = CMDF_WRITE,
		.start_src = TRIG_INT,
		.start_arg = 0,
		.scan_begin_src = TRIG_TIMER,
		.scan_begin_arg = 1e9 / c->out.rate,
		.convert_src = TRIG_NOW,
		.convert_arg = 0,
		.scan_end_src = TRIG_COUNT,
		.scan_end_arg = c->out.chanlist_len,
		.stop_src = TRIG_NONE,
		.stop_arg = 0,
		.chanlist = c->out.chanlist,
		.chanlist_len = c->out.chanlist_len,
	};
#endif

	return 0;
}

static int comedi_stop_in(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction *d = &c->in;

	ret = comedi_unlock(c->it, d->subdevice);
	if (ret)
		error("Failed to lock subdevice %d for node '%s'", d->subdevice, node_name(n));

	return 0;
}

static int comedi_stop_out(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;
	struct comedi_direction *d = &c->out;

	ret = comedi_unlock(c->it, d->subdevice);
	if (ret)
		error("Failed to lock subdevice %d for node '%s'", d->subdevice, node_name(n));

	return 0;
}

int comedi_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;

	const char *device;

	json_t *json_in = NULL;
	json_t *json_out = NULL;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s }",
		"device", &device,
		"in", &json_in,
		"out", &json_out
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (json_in) {
		ret = comedi_parse_direction(&c->in, json_in);
		if (ret)
			return -1;
	}

	if (json_out) {
		ret = comedi_parse_direction(&c->out, json_out);
		if (ret)
			return -1;
	}

	c->device = strdup(device);

	return 0;
}

char * comedi_print(struct node *n)
{
	struct comedi *c = (struct comedi *) n->_vd;

	char *buf = NULL;

	const char *board = comedi_get_board_name(c->it);
	const char *driver = comedi_get_driver_name(c->it);

	strcatf(&buf, "board=%s, driver=%s, device=%s", board, driver, c->device);

	return buf;
}

int comedi_start(struct node *n)
{
	struct comedi *c = (struct comedi *) n->_vd;

	c->it = comedi_open(c->device);
	if (!c->it) {
		const char *err = comedi_strerror(comedi_errno());
		error("Failed to open device: %s", err);
	}

	comedi_start_in(n);
	comedi_start_out(n);

	return 0;
}

int comedi_stop(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;

	comedi_stop_in(n);
	comedi_stop_out(n);

	ret = comedi_close(c->it);
	if (ret)
		return ret;

	return 0;
}

int comedi_read(struct node *n, struct sample *smps[], unsigned cnt)
{
//	struct comedi *c = (struct comedi *) n->_vd;

	return -1;
}

int comedi_write(struct node *n, struct sample *smps[], unsigned cnt)
{
//	struct comedi *c = (struct comedi *) n->_vd;

	return -1;
}

int comedi_fd(struct node *n)
{
	struct comedi *c = (struct comedi *) n->_vd;

	return comedi_fileno(c->it);
}

static struct plugin p = {
	.name		= "comedi",
	.description	= "Comedi-compatible DAQ/ADC cards",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 0,
		.size		= sizeof(struct comedi),
		.parse		= comedi_parse,
		.print		= comedi_print,
		.start		= comedi_start,
		.stop		= comedi_stop,
		.read		= comedi_read,
		.write		= comedi_write,
//		.create		= comedi_create,
//		.destroy	= comedi_destroy,
		.fd		= comedi_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
