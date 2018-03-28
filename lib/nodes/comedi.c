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

int comedi_parse(struct node *n, json_t *cfg)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: o, s?: o, s?: s }",
		"publish", &json_pub,
		"subscribe", &json_sub,
		"format", &format
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	return 0;
}

char * comedi_print(struct node *n)
{
	struct comedi *c = (struct comedi *) n->_vd;

	char *buf = NULL;

	return buf;
}

int comedi_start(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;

	return 0;
}

int comedi_stop(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;

	return 0;
}

int comedi_deinit()
{
	return 0;
}

int comedi_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct comedi *c = (struct comedi *) n->_vd;

	return -1;
}

int comedi_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;

	return cnt;
}

int comedi_fd(struct node *n)
{
	int ret;
	struct comedi *c = (struct comedi *) n->_vd;

	return fd;
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
		.deinit		= comedi_deinit,
		.read		= comedi_read,
		.write		= comedi_write,
		.fd		= comedi_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)
